local moon = require("moon")
local seri = require("seri")

local conf = ...

local pack = seri.pack
local packs = seri.packs
local unpack = seri.unpack
local co_yield = coroutine.yield

local PTYPE_CLUSTER = 20

moon.register_protocol(
{
    name = "cluster",
    PTYPE = PTYPE_CLUSTER,
    pack = pack,
    unpack = unpack
})

local SERVERID = math.tointeger(moon.get_env("SID"))

local function cluster_service()

    local json = require("json")
    local buffer = require("buffer")
    ---@type message
    local message = require("message")
    local socket = require("moon.socket")

    local print = print
    local assert = assert
    local pairs = pairs
    local strfmt = string.format

    local unpack_one = seri.unpack_one
    local queryservice = moon.queryservice
    local wfront = buffer.write_front

    local redirect = message.redirect

    local clusters = {}

    local close_watch = {}
    local send_watch = {}

    local function add_send_watch( fd, sender, sessionid )
        local senders = send_watch[fd]
        if not senders then
            senders = {}
            send_watch[fd] = senders
        end
        local key = (sessionid<<32)|sender
        --print("addkey", key, sessionid)
        senders[key] = moon.time()
    end

    local function remove_send_watch( fd, sender, sessionid )
        local senders = send_watch[fd]
        if not senders then
            assert(false)
            return
        end

        local key = (sessionid<<32)|sender
        --print("remove key", key, sessionid)
        senders[key] = nil
        return true
    end

    moon.async(function()
        for _,senders in pairs(send_watch) do
            for key, t in pairs(senders) do
                if moon.time() - t > 10 then
                    local sender = key&0xFFFFFFFF
                    local sessionid = (key>>32)
                    moon.response("lua", sender, -sessionid, packs(false, "timeout"))
                    senders[key] = nil
                end
            end
        end
        moon.sleep(5000)
    end)

    local services_address = {}
    local function get_service_address(sname)
        local addr = services_address[sname]
        if not addr then
            addr = queryservice(sname)
            assert(addr>0, tostring(sname))
            services_address[sname] = addr
        end
        return addr
    end

    socket.on("connect",function(fd, msg)
        print("connect", fd, moon.decode(msg, "Z"))
    end)

    socket.on("accept",function(fd, msg)
        socket.settimeout(fd, 180)
        print("accept", fd, moon.decode(msg, "Z"))
    end)

    socket.on("message", function(fd, msg)
        local buf = moon.decode(msg, "B")
        ---@type cluster_header
        local header = unpack_one(buf, true)

        if header.session < 0 then -- receive call message
            moon.async(function ()
                local address = get_service_address(header.to_sname)
                assert(address>0, tostring(header.to_sname))
                local session = moon.make_response(address)
                redirect(msg, "", address, moon.PTYPE_LUA, moon.sid(), -session)
                local res = {co_yield()}
                header.session = -header.session
                socket.write(fd, pack(header, table.unpack(res)))
            end)
        elseif header.session > 0 then --receive response message
            if remove_send_watch(fd, header.from_addr, header.session) then
                redirect(msg, "", header.from_addr, moon.PTYPE_LUA, moon.sid(), header.session)
            end
        else-- receive send message
            local address = get_service_address(header.to_sname)
            redirect(msg, "", address, moon.PTYPE_LUA)
        end
    end)

    socket.on("close", function(fd, msg)
        print("close", moon.decode(msg, "Z"))
        for k, v in pairs(close_watch) do
            if v == fd then
                close_watch[k] = false
            end
        end

        local senders = send_watch[fd]
        if senders then
            for key in pairs(senders) do
                local sender = key&0xFFFFFFFF
                local sessionid = (key>>32)
                print("close response", sender, sessionid)
                moon.response("lua", sender, -sessionid, packs(false, "disconnect"))
            end
        end
        send_watch[fd] = nil

        for _,v in pairs(clusters) do
            if v.fd == fd then
                v.fd = false
                break
            end
        end
    end)

    socket.on("error", function(fd, msg)
        print("socket error",fd, moon.decode(msg, "Z"))
    end)

    local function connect(serverid)
        local c = clusters[serverid]
        local fd, err = socket.connect(c.host, c.port, moon.PTYPE_SOCKET)
        if not fd then
            moon.error(err)
            return
        end
        socket.set_enable_chunked(fd, "wr")
        return fd
    end

    local command = {}

    local send_queue = require("moon.queue").new()

    function command.Request(msg)
        ---@type cluster_header
        local header = unpack_one(moon.decode(msg, "B"))

        local fd = clusters[header.to_server].fd

        if fd and socket.write_message(fd, msg) then
            if header.session < 0 then
                --记录mode-call消息，网络断开时，返回错误信息
                add_send_watch(fd, header.from_addr, -header.session)
            end
            return
        end

        send_queue:run(function()
            local data = moon.decode(msg, "Z")
            if not fd then
                fd = connect(header.to_server)
            end

            if fd and socket.write(fd, data) then
                if header.session < 0 then
                    --记录mode-call消息，网络断开时，返回错误信息
                    add_send_watch(fd, header.from_addr, -header.session)
                end
                clusters[header.to_server].fd = fd
            else
                fd = nil
                clusters[header.to_server].fd = false
            end

            if not fd then
                if header.session == 0 then
                    moon.error("not connected cluster")
                else
                    --CASE1:此时与 cluster 连接断开, mode-call, 直接返回错误信息
                    moon.response("lua", header.from_addr, header.session, false, "disconnect")
                end
            end
        end)
    end

    local function load_config()
        local function find_host_port(config_params)
            if not config_params then
                return nil
            end

            local host,port
            for k, v in pairs(config_params) do
                if k == "cluster_host" then
                    host = v
                elseif k == "cluster_port" then
                    port = v
                end
            end
            return host,port
        end

        local content = moon.get_env("CONFIG")
        local js = json.decode(content)
        for _,c in ipairs(js) do
            local host,port = find_host_port(c.params)
            if host and port then
                clusters[c.sid]={host = host, port = port, fd = false}
            end
        end
    end

    load_config()

    if not clusters[SERVERID] then
        print("unconfig cluster node:".. moon.name())
        return false
    end

    moon.dispatch("cluster",function(msg)
        local sender, sessionid, buf = moon.decode(msg, "SEB")
        local cmd = unpack_one(buf, true)
        local fn = command[cmd]
        if fn then
            moon.async(function()
                if sessionid ~= 0 then
                    local unsafe_buf = pack(xpcall(fn, debug.traceback, msg))
                    local ok = unpack_one(unsafe_buf, true)
                    if not ok then
                        wfront(unsafe_buf, packs(false))
                    end
                    moon.raw_send("lua", sender, "", unsafe_buf, sessionid)
                else
                    fn(msg)
                end
            end)
        else
            moon.error(moon.name(), "recv unknown cmd "..tostring(cmd))
        end
    end)

    local listenfd = socket.listen(conf.host, conf.port,moon.PTYPE_SOCKET)
    socket.start(listenfd)
    print(strfmt("cluster run at %s %d", conf.host, conf.port))

    setmetatable(clusters, {__gc=function()
        socket.close(listenfd)
    end})
end

if conf and conf.name then
    cluster_service()
end

local cluster = {}

local cluster_address

---@class cluster_header
---@field public to_server integer @ 接收者服务器ID
---@field public to_sname string @ 接收者服务name
---@field public from_server integer @ 发送者服务器ID
---@field public from_addr integer @ 发送者服务ID
---@field public session integer @ 协程session


function cluster.send(receiver_serverid, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        assert(cluster_address>0)
    end

    local header = {
        to_server = receiver_serverid,
        to_sname = receiver_sname,
        from_server = SERVERID,
        from_addr = moon.sid(),
        session = 0
    }

    moon.send("cluster", cluster_address, "Request", header, ...)
end

function cluster.call(receiver_serverid, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        assert(cluster_address>0)
    end

    local sessionid = moon.make_response(cluster_address)

    local header = {
        to_server = receiver_serverid,
        to_sname = receiver_sname,
        from_server = SERVERID,
        from_addr = moon.sid(),
        session = -sessionid
    }

    moon.raw_send("cluster", cluster_address, "", pack("Request", header, ...))
    return co_yield()
end

return cluster