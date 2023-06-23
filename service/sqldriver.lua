local moon = require("moon")
local seri = require("seri")

local unpack_one = seri.unpack_one

local tbinsert = table.insert

local conf = ...

if conf.name then
    local socket = require("moon.socket")
    local provider = require(conf.provider)
    local list = require("list")

    local clone = moon.clone
    local release = moon.release

    ---@param sql message_ptr
    local function exec_one(db, sql , sender, sessionid)
        while true do
            if db then
                local res = db:query(sql)
                local code = rawget(res, "code")
                if code == "SOCKET" then -- socket error
                    ---socket may disconnect when query , try reconnect
                    db:disconnect()
                    db = nil
                else
                    ---query success but may has sql error
                    if sessionid == 0 and code then
                        moon.error(moon.decode(sql, "Z") ..  "\n" ..table.tostring(res))
                    else
                        moon.response("lua", sender, sessionid, res)
                    end
                    return db
                end
            else
                db = provider.connect(conf.opts)
                if rawget(db, "code") then
                    if sessionid == 0 then
                        ---if execute operation print error, then reconnect
                        moon.error(table.tostring(db))
                    else
                        ---if query operation return socket error or auth error
                        moon.response("lua", sender, sessionid, db)
                        return
                    end
                    db = nil
                    ---sleep then reconnect
                    moon.sleep(1000)
                    moon.error("db reconnecting...", table.tostring(conf.opts))
                end
            end
        end
    end

    local db_pool_size = conf.poolsize or 1

    local traceback = debug.traceback
    local xpcall = xpcall

    local free_all = function(one)
        for _, req in pairs(one.queue) do
            if type(req[1])=="userdata" then
                release(req[1])
            end
        end
        one.queue = {}
    end

    local pool = {}

    for _=1,db_pool_size do
        local one = setmetatable({queue = list.new(), running = false, db = false},{
            __gc = free_all
        })
        tbinsert(pool,one)
    end

    local function execute(sql, hash, sender, sessionid)
        hash = hash%db_pool_size + 1
        --print(moon.name, "db hash", hash, db_pool_size)
        local ctx = pool[hash]
        list.push(ctx.queue, {clone(sql), sender, sessionid})
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
                release(req[1])
            end
            ctx.running = false
        end)
    end

    local fd = socket.sync_connect(conf.opts.host, conf.opts.port, moon.PTYPE_SOCKET_TCP)
    assert(fd, string.format("connect failed provider: %s host: %s port: %s", conf.provider, conf.opts.host, conf.opts.port))
    socket.close(fd)

    local command = {}

    function command.len()
        local res = {}
        for _,v in ipairs(pool) do
            res[#res+1] = list.size(v.queue)
        end
        return res
    end

    local function xpcall_ret(ok, ...)
        if ok then
            return moon.pack(...)
        end
        return moon.pack(false, ...)
    end

    moon.raw_dispatch('lua', function(msg)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd, sz, len = unpack_one(buf, true)
        if cmd == "Q" then
            local hash = unpack_one(buf, true)
            provider.pack_query_buffer(buf)
            execute(msg, hash, sender, sessionid)
            return
        end

        local fn = command[cmd]
        if fn then
            if sessionid == 0 then
                fn(sender, sessionid, moon.unpack(sz, len))
            else
                moon.async(function()
                    local unsafe_buf = xpcall_ret(xpcall(fn, debug.traceback, moon.unpack(sz, len)))
                    moon.raw_send("lua", sender, unsafe_buf, sessionid)
                end)
            end
        else
            moon.error(moon.name, "recv unknown cmd "..tostring(cmd))
        end
    end)

    local function wait_all_send()
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
                return
            end
        end
    end

    moon.system("wait_save", function()
        moon.async(function()
            wait_all_send()
            moon.quit()
        end)
    end)
else
    local client = {}

    local json = require("json")
    local buffer = require("buffer")
    local wfront = buffer.write_front
    local raw_send = moon.raw_send
    local packstr = seri.packs
    local concat = json.concat

    function client.execute(db, sql, hash)
        hash = hash or 1
        local buf = concat(sql)
        if not wfront(buf, packstr("Q", hash)) then
            error("buffer has no front space")
        end
        raw_send("lua", db, buf, 0)
    end

    function client.query(db, sql, hash)
        hash = hash or 1
        local sessionid = moon.make_session(db)
        local buf = concat(sql)
        if not wfront(buf, packstr("Q", hash)) then
            error("buffer has no front space")
        end
        raw_send("lua", db, buf, sessionid)
        return moon.wait(sessionid)
    end

    return client
end

---@class sqlclient
---@field public execute fun(db:integer, sql:string|string[], hash?:integer)
---@field public query fun(db:integer, sql:string|string[], hash?:integer):pg_result|pg_error

