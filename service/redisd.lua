local moon = require("moon")
local seri = require("seri")
local crypt = require("crypt")
local socket = require("moon.socket")
local redis = require("moon.db.redis")

local tbinsert = table.insert

local conf = ...

if conf.name then
    local function connect(opts, auto_reconnect)
        local db, err
        repeat
            db, err = redis.connect(opts)
            if not db then
                moon.error(err)
                break
            end

            if db == redis.socket_error then
                db = nil
            end

            if not db and auto_reconnect then
                moon.sleep(1000)
            end
        until(not auto_reconnect or db)
        return db, err
    end

    ---@param msg buffer_shr_ptr
    local function exec_one(db, msg , sender, sessionid, opt)
        local reconnect_times = 1

        local auto_reconnect = sessionid==0

        if auto_reconnect then
            reconnect_times = -1
        end

        repeat
            local err,res
            if not db then
                db, err = connect(conf.opts, auto_reconnect)
                if not db then
                    if sessionid == 0 then
                        moon.error(err)
                    else
                        moon.response("lua", sender, sessionid, false, err)
                    end
                    return
                end
            end

            if opt == "Q" then
                res, err = redis.raw_send(db, msg)
            else
                local t = {moon.unpack(moon.decode_ref_buffer(msg,"C"))}
                res, err =db[t[1]](db, table.unpack(t, 2))
            end

            if redis.socket_error == res then
                db = nil
                moon.error(err)
                if reconnect_times == 0 then
                    if sessionid ~= 0 then
                        moon.response("lua", sender, sessionid, false, err)
                    end
                    return
                end
            else
                if sessionid == 0 and not res then
                    moon.error(err, crypt.base64encode(moon.decode_ref_buffer(msg, 'Z')))
                else
                    if err then
                        moon.response("lua", sender, sessionid, res, err)
                    else
                        moon.response("lua", sender, sessionid, res)
                    end
                end
                return db, res
            end
        until false
    end

    local db_pool_size = conf.poolsize or 1

    local list = require("list")

    local traceback = debug.traceback
    local xpcall = xpcall

    local clone = moon.ref_buffer
    local release = moon.unref_buffer

    local free_all = function(one)
        for k, req in pairs(one.queue) do
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

    local function docmd(hash, sender, sessionid, msg, opt)
        hash = hash%db_pool_size + 1
        --print(moon.name, "db hash", hash)
        local ctx = pool[hash]
        list.push(ctx.queue, {clone(msg), sender, sessionid, opt})
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
                local iscall = req[3]~=0
                local ok,db = xpcall(exec_one, traceback, ctx.db, req[1], req[2], req[3], req[4])
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
                    release(req[1])
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

    moon.raw_dispatch('lua',function(msg)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd, sz, len = seri.unpack_one(buf, true)
        if cmd == "Q" or cmd == "D" then
            local hash = seri.unpack_one(buf, true)
            docmd(hash, sender, sessionid, msg, cmd)
            return
        end

        local fn = command[cmd]
        if fn then
            moon.async(function()
                if sessionid == 0 then
                    fn(sender, sessionid, buf, msg)
                else
                    if sessionid ~= 0 then
                        local unsafe_buf = xpcall_ret(xpcall(fn, debug.traceback, moon.unpack(sz, len)))
                        moon.raw_send("lua", sender, unsafe_buf, sessionid)
                    end
                end
            end)
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
    local json = require("json")
    local buffer = require("buffer")
    local concat_resp = json.concat_resp
    ---@class redis_client
    local client = {}

    local raw_send = moon.raw_send
    local packstr = seri.packs

    local wfront = buffer.write_front

    --- - if success return value same as redis commands.see http://www.redis.cn/commands/hgetall.html
    --- - if failed return false and error message.
    --- redisd.call(redis_db, "GET", "HELLO")
    function client.call(db, ...)
        local buf, hash = concat_resp(...)
        if not wfront(buf, packstr("Q", hash)) then
            error("buffer has no front space")
        end
        return moon.wait(raw_send("lua", db, buf, moon.next_sequence()))
    end

    function client.direct(db, cmd, ...)
        local buf = seri.pack("D", 1, cmd, ...)
        return moon.wait(raw_send("lua", db, buf, moon.next_sequence()))
    end

    --- redisd.send(redis_db, "SET", "HELLO", "WORLD")
    function client.send(db, ...)
        local buf, hash = concat_resp(...)
        if not wfront(buf, packstr("Q", hash)) then
            error("buffer has no front space")
        end
        raw_send("lua", db, buf)
    end

    return client
end

