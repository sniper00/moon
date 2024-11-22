local moon = require("moon")
local seri = require("seri")
local json = require("json")
local buffer = require("buffer")
local socket = require("moon.socket")
local redis = require("moon.db.redis")

local tbinsert = table.insert

local conf = ...

if conf.name then
    ---@param cmd buffer_shr_ptr
    local function exec_one(db, cmd, sender, sessionid, opt)
        local faild_times = 0
        local err, res
        while true do
            if db then
                if opt == "Q" then
                    res, err = redis.raw_send(db, cmd)
                else
                    local t = { moon.unpack(buffer.unpack(cmd, "C")) }
                    res, err = db[t[1]](db, table.unpack(t, 2))
                end
                if redis.socket_error == res then
                    --- A socket error may occur during the query, close the connection and trigger a reconnect
                    db:disconnect()
                    db = nil
                    faild_times = faild_times + 1
                else
                    if sessionid == 0 and not res then
                        moon.error(err, moon.escape_print(buffer.unpack(cmd)))
                    else
                        moon.response("lua", sender, sessionid, res, err)
                    end
                    return db, res
                end
            else
                if err and faild_times > 1 then
                    moon.error(json.encode {
                        error = string.format("A network error occurred %s, reconnecting db.", err),
                        opts = conf.opts,
                        request = moon.escape_print(buffer.unpack(cmd))
                    })
                end
                db, err = redis.connect(conf.opts)
                if not db then
                    if sessionid ~= 0 then
                        moon.response("lua", sender, sessionid, false, err)
                        return
                    end
                    db = nil
                    faild_times = faild_times + 1
                    moon.sleep(1000)
                end
            end
        end
    end

    local db_pool_size = conf.poolsize or 1

    local list = require("list")

    local traceback = debug.traceback
    local xpcall = xpcall

    local clone = buffer.to_shared

    local pool = {}

    for _ = 1, db_pool_size do
        local one = { queue = list.new(), running = false, db = false }
        tbinsert(pool, one)
    end

    local function docmd(sender, sessionid, args)
        local opt = args[1]
        local hash = args[2]
        hash = hash % db_pool_size + 1
        --print(moon.name, "db hash", hash)
        local ctx = pool[hash]
        list.push(ctx.queue, { clone(args[3]), sender, sessionid, opt })
        if ctx.running then
            return
        end
        ctx.running = true
        moon.async(function()
            while true do
                ---args - sender - sessionid
                local req = list.front(ctx.queue)
                if not req then
                    break
                end
                local iscall = req[3] ~= 0
                local ok, db = xpcall(exec_one, traceback, ctx.db, req[1], req[2], req[3], req[4])
                if not ok then
                    if ctx.db then
                        ctx.db:disconnect()
                        ctx.db = nil
                    end
                    ---print lua error
                    moon.error(db)
                else
                    ctx.db = db
                end
                if iscall or db then
                    list.pop(ctx.queue)
                end
            end
            ctx.running = false
        end)
    end

    assert(socket.try_open(conf.opts.host, conf.opts.port, true),
        string.format("redisd connect %s:%s failed", conf.opts.host, conf.opts.port))

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
                local all = true
                for _, v in ipairs(pool) do
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
        if cmd == "Q" or cmd == "D" then
            docmd(sender, sessionid, args)
            return
        end

        local fn = command[cmd]
        if fn then
            moon.async(function()
                if sessionid == 0 then
                    fn(table.unpack(args, 2))
                else
                    if sessionid ~= 0 then
                        local unsafe_buf = xpcall_ret(xpcall(fn, debug.traceback, table.unpack(args, 2)))
                        moon.raw_send("lua", sender, unsafe_buf, sessionid)
                    end
                end
            end)
        else
            moon.error(moon.name, "recv unknown cmd " .. tostring(cmd))
        end
    end)
else
    local json = require("json")

    local concat_resp = json.concat_resp

    ---@class redis_client
    local client = {}

    --- - if success return value same as redis commands.see http://www.redis.cn/commands/hgetall.html
    --- - if failed return false and error message.
    --- redisd.call(redis_db, "GET", "HELLO")
    function client.call(db, ...)
        local buf, hash = concat_resp(...)
        return moon.call("lua", db, "Q", hash, buf)
    end

    function client.direct(db, cmd, ...)
        return moon.call("lua", db, "D", 1, seri.pack(cmd, ...))
    end

    --- redisd.send(redis_db, "SET", "HELLO", "WORLD")
    function client.send(db, ...)
        local buf, hash = concat_resp(...)
        moon.send("lua", db, "Q", hash, buf)
    end

    return client
end
