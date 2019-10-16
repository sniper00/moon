local seri = require("seri")
local json = require("json")
local moon = require("moon")
local log = require("moon.log")
local socket = require("moon.socket")

local clusters = {}

local print = print
local assert = assert
local error = error
local tostring = tostring
local pairs = pairs
local strpack = string.pack
local strfmt = string.format
local co_yield = coroutine.yield

local packs = seri.packs
local unpack = seri.unpack
local concat = seri.concat
local unpack_header = moon.unpack_cluster_header
local queryservice = moon.queryservice

local close_watch = {}
local send_watch = {}
local connectors = {}

local function add_send_watch( fd, sender, sessionid )
    local senders = send_watch[fd]
    if not senders then
        senders = {}
        send_watch[fd] = senders
    end

    local key = ((-sessionid)<<32)|sender
    senders[key] = true
end

local function remove_send_watch( fd, sender, sessionid )
    local senders = send_watch[fd]
    if not senders then
        assert(false)
        return
    end

    local key = ((-sessionid)<<32)|sender
    senders[key] = nil
end

local function make_message(header, data)
    return concat(strpack("<H",#data),data, header)
end

socket.on("connect",function(fd, msg)
    print("connect", fd, msg:bytes())
end)

socket.on("accept",function(fd, msg)
    socket.settimeout(fd, 10)
    print("accept", fd, msg:bytes())
end)

socket.on("message", function(fd, msg)
    local saddr, rnode, raddr, rsessionid = unpack_header(msg:buffer())
    local receiver = queryservice(raddr)
    if 0 == receiver then
        local err = strfmt( "cluster : tcp message queryservice %s can not find",raddr)
        log.warn(err)

        if 0>rsessionid then
            local rheader = packs(rnode, raddr, saddr, -rsessionid)
            local s = make_message(rheader, packs(false, err))
            socket.write(fd, s)
        end
        return
    end

    --被调用者
    if 0 > rsessionid then
        moon.async(
            function()
                local sessionid = moon.make_response(receiver)
                if not sessionid then
                    local rheader = packs(rnode, raddr, saddr, -rsessionid)
                    local s = make_message(rheader, packs(false, "service dead"))
                    socket.write(fd, s)
                    return
                end

                msg:resend(moon.sid(),receiver,"",sessionid,moon.PTYPE_LUA)

                close_watch[sessionid] = fd
                local res,err2 = co_yield()
                local state = close_watch[sessionid]
                close_watch[sessionid] = nil
                if state == false then
                    return
                end

                if res then
                    res:write_front(strpack("<H",res:size()))
                    local rheader = packs(rnode, raddr, saddr, -rsessionid)
                    res:write_back(rheader)
                    socket.write_message(fd, res)
                else
                    local rheader = packs(rnode, raddr, saddr, -rsessionid)
                    local s = make_message(rheader, packs(false, err2))
                    socket.write(fd, s)
                end
            end
        )
    else
        --调用者
        remove_send_watch(fd,receiver,-rsessionid)
        msg:resend(moon.sid(),receiver,"",-rsessionid,moon.PTYPE_LUA)
    end
end)

socket.on("close", function(fd, msg)
    print("close", msg:bytes())
    for k, v in pairs(close_watch) do
        if v == fd then
            close_watch[k] = false
        end
    end

    local senders = send_watch[fd]
    if senders then
        for key,_ in pairs(senders) do
            local sender = key&0xFFFFFFFF
            local sessionid = -(key>>32)
            print("response", sender, sessionid)
            moon.response("lua", sender, sessionid, packs(false, "connect closed"))
        end
    end

    for k,v in pairs(connectors) do
        if v == fd then
            connectors[k] = nil
            print("connectors remove")
        end
    end
end)

socket.on("error", function(fd, msg)
    print("socket_error",fd, msg:bytes())
end)

local command = {}

command.CALL = function(sender, sessionid,rnode, raddr, msg)
    local fd = connectors[rnode]
    local err
    if not fd then
        local addr = clusters[rnode]
        if not addr then
            err = strfmt("send to unknown node:%s", rnode)
        else
            fd = socket.sync_connect(addr.host, addr.port, moon.PTYPE_SOCKET)
            if 0~=fd then
                connectors[rnode] = fd
            else
                fd = nil
                err = strfmt("connect node:%s failed", rnode)
            end
        end
    end

    if fd then
        msg:write_front(strpack("<H",msg:size()))
        msg:write_back(packs(sender, rnode, raddr, sessionid))
        add_send_watch(fd,sender,sessionid)
        if socket.write_message(fd, msg) then
            return
        else
            err = strfmt("send to %s failed", rnode)
        end
    end

    if sessionid ~= 0 then
        moon.response("lua", sender, sessionid, packs(false, err))
    end
    print("clusterd call error:", err)
end

local function docmd(sender, sessionid, CMD, rnode, raddr, msg)
    local cb = command[CMD]
    if cb then
        cb(sender, sessionid, rnode, raddr ,msg)
    else
        error(strfmt("Unknown command %s", tostring(CMD)))
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

    local content = moon.get_env("CONFIG")
    local js = json.decode(content)
    for _,server in pairs(js) do
        local s = find_service("clusterd",server.services)
        if s then
            local name = server.name
            clusters[name]={sid=server.sid,host=s.host,port=s.port}
        end
    end
end

local conf = ...

load_config()

local name = moon.get_env("SERVER_NAME")
if not clusters[name] then
    print("unconfig cluster node:".. moon.name())
    return false
end

moon.register_protocol(
{
    name = "lua",
    PTYPE = moon.PTYPE_LUA,
    pack = function(...)return ...end,
    unpack = function(...) return ... end,
    dispatch =  function(msg, _)
        local sender = msg:sender()
        local sessionid = msg:sessionid()
        local rnode, raddr, CMD = unpack(msg:header())
        docmd(sender, sessionid, CMD, rnode, raddr, msg)
    end
})

moon.start(function()
    local listenfd = socket.listen(conf.host, conf.port,moon.PTYPE_SOCKET)

    socket.start(listenfd)

    print(strfmt("cluster run at %s %d", conf.host, conf.port))
end)
