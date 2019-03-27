local seri = require("seri")
local json = require("json")
local moon = require("moon")
local log = require("moon.log")
local tcp = require("moon.net.tcpserver")

local clusters = {}

local string_pack = string.pack
local co_yield = coroutine.yield

local packs = seri.packs
local unpack = seri.unpack
local concat = seri.concat
local unpack_header = moon.unpack_cluster_header
local unique_service = moon.unique_service
local write_front = moon.buffer.write_front
local write_back = moon.buffer.write_back

local close_watch = {}
local send_watch = {}
local connectors = {}

local function add_send_watch( connid, sender, responseid )
    local senders = send_watch[connid]
    if not senders then
        senders = {}
        send_watch[connid] = senders
    end

    local key = ((-responseid)<<32)|sender
    senders[key] = true
end

local function remove_send_watch( connid, sender, responseid )
    local senders = send_watch[connid]
    if not senders then
        assert(false)
        return
    end

    local key = ((-responseid)<<32)|sender
    senders[key] = nil
end

local function make_message(header, data)
    return concat(string_pack("<H",#data),data, header)
end

tcp.on("connect",function(sessionid, msg)
    print("connect", sessionid, msg:bytes())
end)

tcp.on("accept",function(sessionid, msg)
    print("accept", sessionid, msg:bytes())
end)

tcp.on("message", function(sessionid, msg)
    local saddr, rnode, raddr, rresponseid = unpack_header(msg:buffer())
    local receiver = unique_service(raddr)
    if 0 == receiver then
        local err = string.format( "cluster : tcp message unique_service %s can not find",raddr)
        log.warn(err)

        if 0>rresponseid then
            local rheader = packs(rnode, raddr, saddr, -rresponseid)
            local s = make_message(rheader, packs(false, err))
            tcp.send(sessionid, s)
        end
        return
    end

    --被调用者
    if 0 > rresponseid then
        moon.async(
            function()
                local responseid = moon.make_response(receiver)
                if not responseid then
                    local rheader = packs(rnode, raddr, saddr, -rresponseid)
                    local s = make_message(rheader, packs(false, "service dead"))
                    tcp.send(sessionid, s)
                    return
                end

                msg:resend(moon.sid(),receiver,"",responseid,moon.PTYPE_LUA)

                close_watch[responseid] = sessionid
                local res,err2 = co_yield()
                local state = close_watch[responseid]
                close_watch[responseid] = nil
                if state == false then
                    return
                end

                if res then
                    local buffer = res:buffer()
                    write_front(buffer,string_pack("<H",res:size()))
                    local rheader = packs(rnode, raddr, saddr, -rresponseid)
                    write_back(buffer,rheader)
                    tcp.send_message(sessionid, res)
                else
                    local rheader = packs(rnode, raddr, saddr, -rresponseid)
                    local s = make_message(rheader, packs(false, err2))
                    tcp.send(sessionid, s)
                end
            end
        )
    else
        --调用者
        remove_send_watch(sessionid,receiver,-rresponseid)
        msg:resend(moon.sid(),receiver,"",-rresponseid,moon.PTYPE_LUA)
    end
end)

tcp.on("close", function(sessionid, msg)
    print("close", msg:bytes())
    for k, v in pairs(close_watch) do
        if v == sessionid then
            close_watch[k] = false
        end
    end

    local senders = send_watch[sessionid]
    if senders then
        for key,_ in pairs(senders) do
            local sender = key&0xFFFFFFFF
            local responseid = -(key>>32)
            print("response", sender, responseid)
            moon.response("lua", sender, responseid, packs(false, "connect closed"))
        end
    end

    for k,v in pairs(connectors) do
        if v == sessionid then
            connectors[k] = nil
            print("connectors remove")
        end
    end
end)

tcp.on("error", function(sessionid, msg)
    print("socket_error",sessionid, msg:bytes())
end)

local command = {}

command.CALL = function(sender, responseid,rnode, raddr, msg)
    local connid = connectors[rnode]
    local err
    if not connid then
        local addr = clusters[rnode]
        if not addr then
            err = string.format("send to unknown node:%s", rnode)
        else
            connid = tcp.connect(addr.ip, addr.port)
            if 0~=connid then
                connectors[rnode] = connid
            else
                connid = nil
                err = string.format("connect node:%s failed", rnode)
            end
        end
    end

    if connid then
        local buffer = msg:buffer()
        write_front(buffer,string_pack("<H",msg:size()))
        write_back(buffer,packs(sender, rnode, raddr, responseid))
        add_send_watch(connid,sender,responseid)
        if tcp.send_message(connid, msg) then
            return
        else
            err = string.format("send to %s failed", rnode)
        end
    end

    if responseid ~= 0 then
        moon.response("lua", sender, responseid, packs(false, err))
    end
    print("clusterd call error:", err)
end

local function docmd(sender, responseid, CMD, rnode, raddr, msg)
    local cb = command[CMD]
    if cb then
        cb(sender, responseid, rnode, raddr ,msg)
    else
        error(string.format("Unknown command %s", tostring(CMD)))
    end
end

local function load_config()
    local function find_service(sname,services)
        if not services then
            return nil
        end

        for _, s in pairs(services) do
            if s.name==sname then
                return s
            end
        end
        return nil
    end

    local content = moon.get_env("server_config")
    local js = json.decode(content)
    for _,server in pairs(js) do
        local s = find_service("clusterd",server.services)
        if s then
            local name = server.name
            clusters[name]={sid=server.sid,ip=s.network.ip,port=s.network.port}
        end
    end
end

moon.init(function(  )
    load_config("config.json")
    local name = moon.get_env("name")
    if not clusters[name] then
        print("unconfig node:".. moon.name())
        return false
    end

    tcp.settimeout(10)

    moon.register_protocol(
    {
        name = "lua",
        PTYPE = moon.PTYPE_LUA,
        pack = function(...)return ...end,
        unpack = function(...) return ... end,
        dispatch =  function(msg, _)
            local sender = msg:sender()
            local responseid = msg:responseid()
            local rnode, raddr, CMD = unpack(msg:header())
            docmd(sender, responseid, CMD, rnode, raddr, msg)
        end
    })
    return true
end)

