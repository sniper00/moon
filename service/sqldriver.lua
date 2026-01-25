local moon     = require("moon")
local buffer   = require("buffer")
local json     = require("json")
local tbinsert = table.insert

-- Module configuration passed via `conf` from service launcher.
-- `conf` expected fields:
--   - name: (optional) when set, this module acts as the provider side
--           (accepting and executing queries using `conf.provider`).
--   - provider: name of the provider module (e.g. "pg"), used when
--               running in provider mode.
--   - opts: provider connection options (host/port/user/password/dbname).
--   - poolsize: number of per-service queues (default 1).
--
-- This file implements two roles depending on `conf.name`:
-- 1) Provider/service side: manages a pool of DB connections/queues and
--    executes queries received from other services. This includes retry
--    logic and reconnection handling.
-- 2) Client side (when `conf.name` is nil): convenience helpers that
--    forward queries to the running sqldriver service via `moon.send`
--    / `moon.call` (remote RPC).

local conf     = ...

---@class SqlClient
local client = {}

if conf.name then
    local socket = require("moon.socket")
    local provider = require(conf.provider)
    local list = require("list")

    local clone = buffer.to_shared

    --- Execute a single query on a DB connection with retry and reconnect.
    ---
    --- Behavior:
    ---  - If `db` is provided, call `provider[cmd](db, sql)` to execute.
    ---  - On socket-level errors (res.code == "SOCKET") the connection is
    ---    closed and the function will attempt to reconnect and retry.
    ---  - If the provider returns an error and `sessionid == 0` the error
    ---    is logged with stack info; otherwise the response is sent back
    ---    to the requester via `moon.response`.
    ---  - On permanent failures the function returns without responding
    ---    (the caller handles returning error objects when appropriate).
    ---
    --- Parameters:
    ---  - db: provider connection object or `nil` if not connected yet.
    ---  - sql: query buffer (shared pointer) or raw SQL string/table.
    ---  - sender: service id of the caller (used for `moon.response`).
    ---  - sessionid: RPC session id (0 means no response expected).
    ---  - cmd: provider command name, e.g. "query" or "query_params".
    ---
    --- Returns:
    ---  - db: the (possibly reconnected) db connection to keep in pool.
    ---@param sql buffer_shr_ptr
    local function exec_one(db, sql, sender, sessionid, cmd)
        local faild_times = 0
        local err
        while true do
            if db then
                local res = provider[cmd](db, sql)
                local code = rawget(res, "code")
                if code == "SOCKET" then -- socket error
                    err = res.message
                    --- A socket error may occur during the query, close the connection and trigger a reconnect
                    db:disconnect()
                    db = nil
                    faild_times = faild_times + 1
                else
                    --- Query succeeded, but there may be SQL errors
                    if sessionid == 0 and code then
                        moon.error(moon.escape_print(buffer.unpack(sql)) .. "\n" .. table.tostring(res))
                    else
                        moon.response("lua", sender, sessionid, res)
                    end
                    return db
                end
            else
                if err and faild_times > 1 then
                    moon.error(json.encode {
                        error = string.format("A network error occurred %s, reconnecting db.", err),
                        opts = conf.opts,
                        request = moon.escape_print(buffer.unpack(sql))
                    })
                end

                db = provider.connect(conf.opts)
                if rawget(db, "code") then
                    err = rawget(db, "message")
                    if sessionid ~= 0 then
                        ---if query operation return socket error or auth error
                        moon.response("lua", sender, sessionid, db)
                        return
                    end
                    db = nil
                    faild_times = faild_times + 1
                    ---sleep then reconnect
                    moon.sleep(1000)
                end
            end
        end
    end

    -- Number of per-hash queues (controls concurrency). Incoming requests
    -- are assigned to a queue using `hash % db_pool_size + 1`. This helps
    -- distribute load across multiple DB connections / workers.
    local db_pool_size = conf.poolsize or 1

    local traceback = debug.traceback
    local xpcall = xpcall

    -- Pool holds `db_pool_size` contexts. Each context contains:
    --  - queue: a FIFO of pending requests
    --  - running: whether a worker coroutine is currently processing the queue
    --  - db: the active DB connection for that context (or nil)
    local pool = {}

    for _ = 1, db_pool_size do
        local one = { queue = list.new(), running = false, db = false }
        tbinsert(pool, one)
    end

    -- Enqueue a request and ensure a worker is processing the queue.
    -- `args` format: { cmd, hash, sql_or_buffer }
    -- Worker loop processes items one-by-one by popping from the queue and
    -- calling `exec_one`. Any returned `db` connection is stored back into
    -- the context for reuse.
    local function execute(sender, sessionid, args)
        local hash = args[2] % db_pool_size + 1
        local ctx = pool[hash]
        local sql = type(args[3]) == "userdata" and clone(args[3]) or args[3]
        list.push(ctx.queue, { sql, sender, sessionid, args[1] })

        if ctx.running then
            return
        end

        ctx.running = true
        moon.async(function()
            while true do
                local req = list.pop(ctx.queue)
                if not req then
                    break
                end
                local ok, db = xpcall(exec_one, traceback, ctx.db, req[1], req[2], req[3], req[4])
                if ok then
                    ctx.db = db
                else
                    moon.error(db)
                    if ctx.db then
                        ctx.db:disconnect()
                    end
                    ctx.db = nil
                end
            end
            ctx.running = false
        end)
    end

    assert(socket.try_open(conf.opts.host, conf.opts.port, true),
        string.format("connect failed provider: %s host: %s port: %s", conf.provider, conf.opts.host, conf.opts.port))


    local command = {}

    function command.len()
        local res = {}
        for _, v in ipairs(pool) do
            res[#res + 1] = list.size(v.queue)
        end
        return res
    end

    function command.save_then_quit()
        moon.async(function()
            while true do
                local all_empty = true
                for i, v in ipairs(pool) do
                    if list.front(v.queue) then
                        all_empty = false
                        print("wait_all_send", i, list.size(v.queue))
                        break
                    end
                end
                if all_empty then
                    break
                end
                moon.sleep(1000)
            end
            moon.quit()
        end)
    end

    local function xpcall_ret(ok, ...)
        return moon.pack(ok and ... or false, ...)
    end

    -- Raw dispatcher for incoming 'lua' messages.
    -- Message format decoded by `moon.decode(msg, "SEC")` yields
    --    sender, sessionid, sz, len
    -- where `sz,len` are used by `moon.unpack` to reconstruct the args
    -- table. `args[1]` is the command name.
    --
    -- If `provider[cmd]` exists we treat it as a DB operation (query,
    -- query_params, pipe, etc.) and forward to `execute()` to enqueue
    -- and process it in the pool.
    --
    -- Otherwise, if `command[cmd]` exists we invoke it directly. If the
    -- caller expects a response (`sessionid ~= 0`) we run the handler in
    -- an async coroutine and return the packed result via `moon.raw_send`.
    moon.raw_dispatch('lua', function(msg)
        local sender, sessionid, sz, len = moon.decode(msg, "SEC")

        local args = { moon.unpack(sz, len) }

        local cmd = args[1]
        if provider[cmd] then
            if cmd == "query" then
                provider.pack_query_buffer(args[3])
            end
            execute(sender, sessionid, args)
            return
        end

        local fn = command[cmd]
        if fn then
            if sessionid == 0 then
                fn(sender, sessionid, table.unpack(args, 2))
            else
                moon.async(function()
                    local unsafe_buf = xpcall_ret(xpcall(fn, debug.traceback, table.unpack(args, 2)))
                    moon.raw_send("lua", sender, unsafe_buf, sessionid)
                end)
            end
        else
            moon.error(moon.name, "recv unknown cmd " .. tostring(cmd))
        end
    end)
else
    -- Client-side helpers: when this module is required without `conf.name`
    -- it exports lightweight functions that send/call the sqldriver service
    -- to perform queries. These are convenience wrappers used by other
    -- services to interact with the DB without embedding DB driver logic.
    --
    -- Note on `hash` argument: a numeric `hash` (defaults to 1) is used by
    -- the sqldriver to choose which queue/worker (pool slot) will process
    -- the request: `index = hash % db_pool_size + 1`. This allows callers
    -- to route related requests to the same worker (affinity) if needed.
    local concat = json.concat

    --- Send a query (no response expected).
    ---@param db integer service ID of the sqldriver
    ---@param sql string|table SQL text or table that `json.concat` accepts
    ---@param hash? integer optional routing hash (default 1)
    function client.execute(db, sql, hash)
        moon.send("lua", db, "query", hash or 1, concat(sql))
    end

    --- Call a query and wait for the result (synchronous RPC).
    ---@async
    ---@param db integer service ID of the sqldriver
    ---@param sql string|table SQL text or buffer
    ---@param hash? integer optional routing hash (default 1)
    ---@return table provider result (rows/error table)
    function client.query(db, sql, hash)
        return moon.call("lua", db, "query", hash or 1, concat(sql))
    end

    --- Query with parameter binding (Postgres-style $1, $2 placeholders) or (Mysql-style ? placeholders).
    --- Accepts either `client.query_params(db, "select ... $1", param1)` or
    --- a prebuilt table form `{sql, param1, param2, ...}`.
    --- If the SQL contains `$` placeholders it will be converted with
    --- `json.pq_query`; otherwise params are forwarded as-is.
    ---@async
    ---@param db integer service ID
    ---@param sql string|table SQL string or table {sql, ...}
    ---@param ... any parameters to bind
    function client.query_params(db, sql, ...)
        local params = type(sql) == "string" and {sql, ...} or sql
        local query_data = string.find(params[1], "$", 1, true) and json.pq_query(params) or params
        return moon.call("lua", db, "query_params", 1, query_data)
    end

    --- Like `query_params` but fire-and-forget (no response expected).
    ---@param db integer service ID
    ---@param sql string|table SQL string or table {sql, ...}
    ---@param ... any parameters to bind
    function client.execute_params(db, sql, ...)
        local params = type(sql) == "string" and {sql, ...} or sql
        local query_data = string.find(params[1], "$", 1, true) and json.pq_query(params) or params
        moon.send("lua", db, "query_params", 1, query_data)
    end

    --- PostgreSQL Pipe support: execute multiple statements in a single transaction.
    --- `req` should be a list of statements in the form
    --- `{{sql, param1, ...}, {sql, param1, ...}, ...}`. Returns provider
    --- result table on success or error on failure.
    ---@async
    function client.pipe(db, req)
        return moon.call("lua", db, "pipe", 1, json.pq_pipe(req))
    end

    --- PostgreSQL pipe support, with transaction. Fire-and-forget version of `pipe`.
    ---@param db integer @ service ID
    ---@param req table @ {{sql, param1, param2, ...}, {sql, param1, param2, ...}, ...}
    function client.execute_pipe(db, req)
        moon.send("lua", db, "pipe", 1, json.pq_pipe(req))
    end
end

return client
