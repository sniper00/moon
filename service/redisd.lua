local moon = require("moon")
local seri = require("seri")
local socket = require("moon.socket")
local redis = require("moon.db.redis")

local tbinsert = table.insert
local tbremove = table.remove
local tbunpack = table.unpack

local conf = ...

local PTYPE_REDIS = 101

moon.register_protocol({
    name = "redis",
    PTYPE = PTYPE_REDIS,
    pack = seri.pack,
    unpack = seri.unpack,
    dispatch = nil
})

if conf.name then
    local function connect(db_conf, auto)
        local db, err
        repeat
            db, err = redis.connect(db_conf)
            if not db then
                break
            end

            if db == redis.socket_error then
                db = nil
            end

            if not db and auto then
                moon.sleep(1000)
            end
        until(not auto or db)
        return db, err
    end

    local function exec_one(db, args , sender, sessionid, auto)
        repeat
            local err,res
            if not db then
                db, err = connect(conf, auto)
                if not db then
                    if sessionid == 0 then
                        moon.error(err)
                    else
                        moon.response("redis", sender, sessionid, false, err)
                    end
                    return
                end
            end

            local cmd = args[1]
            res, err = db[cmd](db, tbunpack(args,2))
            if redis.socket_error == res then
                db = nil
                if auto then
                    moon.error(err, "will reconnect...")
                    moon.sleep(100)
                else
                    moon.error(err)
                    moon.response("redis", sender, sessionid, false, err)
                    return db, res
                end
            else
                if sessionid == 0 then
                    if not res then
                        moon.error(err)
                    end
                else
                    moon.response("redis", sender, sessionid, res, err)
                end
                return db, res
            end
        until false
    end

    local db_pool_size = conf.poolsize or 1

    local pool = {}

    for _=1,db_pool_size do
        tbinsert(pool,{queue = {}, running = false, db = false})
    end

    local function docmd(hash, sender, sessionid, ...)
        hash = hash%db_pool_size
        if hash == 0 then
            hash = db_pool_size
        end

        --print(moon.name(), "db hash", hash)

        local ctx = pool[hash]
        tbinsert(ctx.queue, {{...}, sender, sessionid})
        if ctx.running then
            return
        end

        moon.async(function()
            ctx.running = true
            while #ctx.queue >0 do
                ---args - sender - sessionid
                local req = ctx.queue[1]
                local iscall = req[3]~=0
                local ok
                ctx.db, ok = exec_one(ctx.db, req[1], req[2], req[3], not iscall)
                if iscall or (ctx.db and ok) then
                    tbremove(ctx.queue,1)
                end
            end
            ctx.running = false
        end)
    end

    local fd = socket.sync_connect(conf.host,conf.port,moon.PTYPE_TEXT)
    assert(fd, "connect db redis failed")
    socket.close(fd)

    moon.dispatch('redis',function(msg,unpack)
        docmd(unpack(msg:header()),msg:sender(), msg:sessionid(), unpack(msg:cstr()))
    end)

    local function wait_all_send()
        while true do
            local all = true
            for _,v in ipairs(pool) do
                if #v.queue >0 then
                    all = false
                    print("wait_all_send", _, #v.queue)
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

    moon.exit(function()
        moon.async(function()
            moon.co_wait_exit(false)
            wait_all_send()
            moon.quit()
        end)
    end)
else

    ---@class redis_client
    local client = {}

    local yield = coroutine.yield
    local raw_send = moon.raw_send
    local pack = seri.pack
    local packstr = seri.packs

    function client.call(db, ...)
        local sessionid = moon.make_response(db)
        raw_send("redis", db, packstr(1), pack(...), sessionid)
        return yield()
    end

    function client.hash_call(hash, db, ...)
        local sessionid = moon.make_response(db)
        hash = hash or 1
        raw_send("redis", db, packstr(hash), pack(...), sessionid)
        return yield()
    end

    function client.send(db, ...)
        raw_send("redis", db, packstr(1), pack(...), 0)
    end

    function client.hash_send(hash, db, ...)
        hash = hash or 1
        raw_send("redis", db, packstr(hash), pack(...), 0)
    end
    return client
end

