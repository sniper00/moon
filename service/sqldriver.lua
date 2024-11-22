local moon = require("moon")
local buffer = require("buffer")
local json = require("json")
local tbinsert = table.insert

local conf = ...

if conf.name then
    local socket = require("moon.socket")
    local provider = require(conf.provider)
    local list = require("list")

    local clone = buffer.to_shared

    ---@param sql buffer_shr_ptr
    local function exec_one(db, sql, sender, sessionid)
        local faild_times = 0
        local err
        while true do
            if db then
                local res = db:query(sql)
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
                        moon.error(moon.escape_print(buffer.unpack(sql)) ..  "\n" ..table.tostring(res))
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

    local db_pool_size = conf.poolsize or 1

    local traceback = debug.traceback
    local xpcall = xpcall

    local pool = {}

    for _=1,db_pool_size do
        local one = {queue = list.new(), running = false, db = false}
        tbinsert(pool,one)
    end

    local function execute(sender, sessionid, args)
        local hash = args[2]
        hash = hash%db_pool_size + 1
        --print(moon.name, "db hash", hash, db_pool_size)
        local ctx = pool[hash]
        list.push(ctx.queue, {clone(args[3]), sender, sessionid})
        if ctx.running then
            return
        end

        ctx.running = true

        moon.async(function()
            while true do
                ---{sql, sender, sessionid}
                local req = list.pop(ctx.queue)
                if not req then
                    break
                end
                local ok, db = xpcall(exec_one, traceback, ctx.db, req[1], req[2], req[3])
                if not ok then
                    ---lua error
                    moon.error(db)
                end
                ctx.db = db
            end
            ctx.running = false
        end)
    end

    assert(socket.try_open(conf.opts.host, conf.opts.port, true),
        string.format("connect failed provider: %s host: %s port: %s", conf.provider, conf.opts.host, conf.opts.port))


    local command = {}

    function command.len()
        local res = {}
        for _,v in ipairs(pool) do
            res[#res+1] = list.size(v.queue)
        end
        return res
    end

    function command.save_then_quit()
        moon.async(function()

            while true do
                local all = true
                for _,v in ipairs(pool) do
                    if list.front(v.queue) then
                        all = false
                        print("wait_all_send", _, list.size(v.queue))
                        break
                    end
                end
    
                if not all then
                    moon.sleep(1000)
                else
                    break
                end
            end

            moon.quit()
        end)
    end

    local function xpcall_ret(ok, ...)
        if ok then
            return moon.pack(...)
        end
        return moon.pack(false, ...)
    end

    moon.raw_dispatch('lua', function(msg)
        local sender, sessionid, sz, len = moon.decode(msg, "SEC")

        local args = { moon.unpack(sz, len) }

        local cmd = args[1]
        if cmd == "Q" then
            provider.pack_query_buffer(args[3])
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
            moon.error(moon.name, "recv unknown cmd "..tostring(cmd))
        end
    end)
else
    local client = {}

    local json = require("json")

    local concat = json.concat

    function client.execute(db, sql, hash)
        moon.send("lua", db, "Q", hash or 1, concat(sql))
    end

    function client.query(db, sql, hash)
        return moon.call("lua", db, "Q", hash or 1, concat(sql))
    end

    return client
end

---@class sqlclient
---@field public execute fun(db:integer, sql:string|string[], hash?:integer)
---@field public query fun(db:integer, sql:string|string[], hash?:integer):pg_result

