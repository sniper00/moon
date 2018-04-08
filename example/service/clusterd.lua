local moon = require("moon")
local seri = require("seri")
local log = require("log")
local json = require("json")
local clusters = {}

local seri_pack = seri.pack
local seri_packstr = seri.packstring
local seri_unpack = seri.unpack
local co_yield = coroutine.yield
local unique_service = moon.unique_service

local network
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

local socket_handler = {}

socket_handler[1] = function(sessionid, msg)
    print("connect",sessionid, msg:bytes())
end

socket_handler[2] = function(sessionid, msg)
    print("accept",sessionid, msg:bytes())
end

socket_handler[3] = function(sessionid, msg)
    local snode, saddr, rnode, raddr, rresponseid, data = seri_unpack(msg:bytes())
    local receiver = unique_service(raddr)
    if 0 == receiver then
        log.warn("cluster : socket_handler[3] unique_service %s can not find",raddr)
        return
    end
    --被调用者
    if 0 > rresponseid then
        moon.start_coroutine(
            function()
                local responseid, err = moon.raw_send("lua", receiver, nil, data)
                local sdata
                if responseid then
                    response_watch[responseid] = sessionid
                    local ret, err = co_yield()
                    local state = response_watch[responseid]
                    response_watch[responseid] = nil
                    if state == 'close' then
                        return
                    end
                    if ret then
                        sdata = seri_packstr(ret)
                    else
                        sdata = seri_packstr(false, err)
                    end
                else
                    sdata = seri_packstr(false, err)
                end
                local t = seri_pack(rnode, raddr, snode, saddr, -rresponseid, sdata)
                network:send(sessionid, t)
            end
        )
    else
        --调用者
        remove_send_watch(sessionid,receiver,-rresponseid)
        moon.raw_send("lua", receiver, nil, data, -rresponseid)
    end
end

--close
socket_handler[4] = function(sessionid, msg)
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
            moon.response("lua", sender, responseid, false, "conn closed")
        end
    end

    for k,v in pairs(connectors) do
        if v == sessionid then
            connectors[k] = nil
            print("connectors remove")
        end
    end
end

socket_handler[5] = function(sessionid, msg)
    print("socket_error",sessionid, msg:bytes())
end

socket_handler[7] = function(sessionid, msg)
    print("socket_logic_error",sessionid, msg:bytes())
end

moon.register_protocol(
    {
        name = "socket",
        PTYPE = moon.PSOCKET,
        pack = function(...)
            return ...
        end,
        dispatch = function(msg)
            local sessionid = msg:sender()
            local subtype = msg:subtype()
            local f = socket_handler[subtype]
            if f then
                f(sessionid, msg)
            end
        end
    }
)

local command = {}

command.CALL = function(sender, responseid, rnode, raddr, ...)
    local connid = connectors[rnode]
    local err
    if not connid then
        local addr = clusters[rnode]
        if not addr then
            err = string.format("send to unknown node:%s", rnode)
        else
            connid = network:connect(addr.ip, addr.port)
            if 0~=connid then
                connectors[rnode] = connid
            else
                connid = nil
                err = string.format("connect node:%s failed", rnode)
            end
        end
    end

    if connid then
        local t = seri.pack(moon.name(), sender, rnode, raddr, responseid, seri_packstr(...))
        add_send_watch(connid,sender,responseid)
        if network:send(connid, t) then
            err = string.format("send to %s failed", rnode)
            return
        end
    end

    if responseid ~= 0 then
        moon.response("lua", sender, responseid, false, err)
    end
    print("clusterd call error:", err)
end

local function docmd(sender, responseid, CMD, ...)
    local cb = command[CMD]
    if cb then
        cb(sender, responseid, ...)
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

    network = moon.get_component_tcp(config.network.name)
    assert(network)
    network:settimeout(10)

    moon.dispatch(
        "lua",
        function(msg, p)
            local sender = msg:sender()
            local responseid = msg:responseid()
            docmd(sender, responseid, p.unpack(msg:bytes()))
        end
    )

    return true
end)

