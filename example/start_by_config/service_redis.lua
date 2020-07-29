local moon = require("moon")
local socket = require("moon.socket")
local queue = require("moon.queue")
local redis = require("moon.db.redis")

local tbinsert = table.insert
local tbremove = table.remove
local tbunpack = table.unpack

local conf = ...

local function connect(db_conf, auto)
    local db, err
    repeat
        db, err = redis.connect(db_conf)
        if not db then
            print("redis connect failed ", err)
        end

        if not db and auto then
            moon.sleep(100)
        end
    until(not auto or db)
    return db, err
end

local function exec_one(db, sender, sessionid, args , auto)
    repeat
        if not db then
            local _db,err = connect(conf, auto)
            if not _db then
                moon.response("lua", sender, sessionid, nil, err)
                return
            end
            db = _db
        end

        local cmd = args[1]
        local res, err = db[cmd](db, tbunpack(args,2))
        if redis.socket_error == res then
            db = nil
            print("redis socket error",res, err)
            if auto then
                moon.sleep(100)
            end
        else
            moon.response("lua", sender, sessionid, res)
            return db
        end
    until false
end

local db_num = conf.connection_num
local balance = 1
local connection_pool = {}

for _=1,db_num do
    tbinsert(connection_pool,{queue = queue.new()})
end

local function call(sender, sessionid, ...)
    local args = {...}

    local ctx = connection_pool[balance]

    balance = balance + 1
    if balance > db_num then
        balance = 1
    end

    ctx.queue:run(function()
        ctx.db = exec_one(ctx.db, sender, sessionid, args)
    end)
end

local wpoolsize = 10

local writepool = {}

for _=1,wpoolsize do
    tbinsert(writepool,{queue = {}, sending = false, db = false})
end

local function send(hash, ...)
    hash = hash%wpoolsize
    if hash == 0 then
        hash = wpoolsize
    end

    local ctx = writepool[hash]

    tbinsert(ctx.queue, {...})
    if ctx.sending then
        return
    end

    moon.async(function()
        ctx.sending = true
        while #ctx.queue >0 do
            local req = ctx.queue[1]
            ctx.db = exec_one(ctx.db, 0, 0, req, true)
            if ctx.db then
                tbremove(ctx.queue,1)
            end
        end
        ctx.sending = false
    end)
end

local fd = socket.sync_connect(conf.host,conf.port,moon.PTYPE_TEXT)
assert(fd, "connect db redis failed")

socket.close(fd)

moon.dispatch('lua',function(msg,unpack)
    if #msg:header() >0 then
        send(unpack(msg:cstr()))
    else
        call(msg:sender(), msg:sessionid(), unpack(msg:cstr()))
    end
end)

moon.exit(function()
    moon.async(function()
        moon.co_wait_exit()
    end)
end)