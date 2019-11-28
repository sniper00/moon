local moon = require("moon")
local socket = require("moon.socket")
local mysql = require("moon.db.mysql")
local queue = require("moon.queue")

local tbinsert = table.insert
local tbremove = table.remove
local tbunpack = table.unpack


local conf = ...

local command = {}

local function connect(conf_, auto)
    local db, err
    repeat
        db, err = mysql.connect(conf_)
        if not db then
            print("mysql connect failed ", err)
        end

        if not db and auto then
            moon.sleep(100)
        end
    until(not auto or db)
    return db, err
end

local db_num = conf.connection_num
local balance = 1
local db_call = {}

for _=1,db_num do
    tbinsert(db_call,{q = queue.new()})
end

function command.query(sender, sessionid, ...)
    local args = {...}

    local db = db_call[balance]

    balance = balance + 1
    if balance > db_num then
        balance = 1
    end

    db.q:run(function()
        if not db.db then
            local db_,err = connect(conf)
            if not db_ then
                --print(err)
                moon.response("lua", sender, sessionid, {err = err})
                return
            end
            db.db = db_
        end

        local res = db.db:query(tbunpack(args))
        if res.err then
            --print("query error", tbunpack(args))
            --print_r(res)
            if not res.errno or res.errno == 1053 then
                db.db, res.err = connect(conf)
            end
            if db.db then
                res = db.db:query(tbunpack(args))
            end
        end
        moon.response("lua", sender, sessionid, res)
    end)
end

local send_queue = {}
local sending = false

local db_send

function command.execute(_,_, ...)
    tbinsert(send_queue, {...})
    if not sending then
        sending = true
        moon.async(function()
            if not db_send then
                local db,err = connect(conf)
                if not db then
                    print(err)
                    return
                end
                db_send = db
            end

            while #send_queue >0 do
                local req = send_queue[1]
                local res = db_send:query(tbunpack(req))
                if res.err then
                    print_r(res)
                    if not res.errno or res.errno == 1053 then
                        print("mysql network error, reconnecting...")
                        db_send, res.err = connect(conf, true)
                    else
                        print(tbunpack(req))
                        break
                    end
                else
                    tbremove(send_queue,1)
                end
            end
            sending = false
        end)
    end
end

local fd = socket.sync_connect(conf.host,conf.port,moon.PTYPE_TEXT)
assert(fd, "connect db mysql failed")

socket.close(fd)

local function docmd(sender, sessionid, cmd, ...)
    local f = command[cmd]
    if f then
        f(sender, sessionid, ...)
    end
end

moon.dispatch('lua',function(msg,p)
    docmd(msg:sender(), msg:sessionid(), p.unpack(msg))
end)

-- moon.exit(function()
--     moon.async(function()
--         moon.co_wait_exit()
--     end)
-- end)