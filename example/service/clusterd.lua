local moon = require("moon")
local seri = require("seri")
local log = require("log")
local json = require("json")
local tcp = require("moon.tcpserver")

local clusters = {}

local pack_cluster = moon.pack_cluster
local unpack_cluster = moon.unpack_cluster

local seri_packstr = seri.packstring
local seri_unpack = seri.unpack
local co_yield = coroutine.yield
local unique_service = moon.unique_service

local response_watch = {}
local send_watch = {}
local connectors = {}

local function add_send_watch( connid, sender, responseid )
    local key = table.concat({tostring(connid),tostring(sender),tostring(responseid)})
    assert(send_watch[key]==nil)
    send_watch[key] = {connid,sender,responseid}
end

local function remove_send_watch( connid, sender, responseid )
    local key = table.concat({tostring(connid),tostring(sender),tostring(responseid)})
    send_watch[key] = nil
end

tcp.on("connect",function(sessionid, msg)
    print("connect", sessionid, msg:bytes())
end)

tcp.on("accept",function(sessionid, msg)
    print("accept", sessionid, msg:bytes())
end)

tcp.on("message", function(sessionid, msg)
    local snode, saddr, rnode, raddr, rresponseid = seri_unpack(unpack_cluster(msg))
    local receiver = unique_service(raddr)
    if 0 == receiver then
        local err = string.format( "cluster : tcp message unique_service %s can not find",raddr)
        log.warn(err)

        if 0>rresponseid then
            local s = moon.make_cluster_message(seri_packstr(rnode, raddr, snode, saddr, -rresponseid),seri_packstr(false, err))
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
                    local s = moon.make_cluster_message(seri_packstr(rnode, raddr, snode, saddr, -rresponseid),seri_packstr(false, "service dead"))
                    tcp.send(sessionid, s)
                    return
                end

                msg:resend(moon.sid(),receiver,"",responseid,moon.PTYPE_LUA)

                response_watch[responseid] = sessionid
                local ret,err2 = co_yield()
                local state = response_watch[responseid]
                response_watch[responseid] = nil
                if state == 'close' then
                    return
                end

                if ret then
                    pack_cluster(seri_packstr(rnode, raddr, snode, saddr, -rresponseid),ret)
                    tcp.send_message(sessionid, ret)
                else
                    local s = moon.make_cluster_message(seri_packstr(rnode, raddr, snode, saddr, -rresponseid),seri_packstr(false, err2))
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
    for k, v in pairs(response_watch) do
        if v == sessionid then
            response_watch[k] = 'close'
        end
    end

    for _, v in pairs(send_watch) do
        local connid = v[1]
        if connid == sessionid then
            local sender = v[2]
            local responseid = v[3]
            print("response", sender, responseid)
            moon.response("lua", sender, responseid, seri_packstr(false, "conn closed"))
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
        pack_cluster(seri_packstr(moon.name(), sender, rnode, raddr, responseid),msg)
        add_send_watch(connid,sender,responseid)
        if tcp.send_message(connid, msg) then
            return
        else
            err = string.format("send to %s failed", rnode)
        end
    end

    if responseid ~= 0 then
        moon.response("lua", sender, responseid, seri_packstr(false, err))
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

moon.init(function( config )
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
            local CMD, rnode, raddr = seri_unpack(msg:header())
            docmd(sender, responseid, CMD, rnode, raddr, msg)
        end
    })
    return true
end)

