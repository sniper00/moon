local moon = require("moon")
local seri = require("seri")
local buffer = require("buffer")

local conf = ...

local pack = seri.pack

local NODE = math.tointeger(moon.env("NODE"))

local function cluster_service()

    local json = require("json")
    local socket = require("moon.socket")
    local httpc = require("moon.http.client")

    local print = print
    local assert = assert
    local pairs = pairs
    local strfmt = string.format

    local unpack_one = seri.unpack_one

    local redirect = moon.redirect

    local clusters = {}

    local close_watch = {}
    local send_watch = {}

    local function add_send_watch( fd, sender, sessionid )
        local senders = send_watch[fd]
        if not senders then
            senders = {}
            send_watch[fd] = senders
        end
        senders[sessionid.."-"..sender] = moon.time()
    end

    local function remove_send_watch( fd, sender, sessionid )
        local senders = send_watch[fd]
        if not senders then
            assert(false)
            return
        end

        senders[sessionid.."-"..sender] = nil
        return true
    end

    local services_address = setmetatable ({} , {
        __index = function(t, key)
            local addr = moon.queryservice(key)
            if addr == 0 then
                return nil
            end
            t[key] = addr
            return addr
        end
    })

    socket.on("connect",function(fd, msg)
        print("connect", fd, moon.decode(msg, "Z"))
        socket.set_enable_chunked(fd,"wr")
    end)

    socket.on("accept",function(fd, msg)
        socket.settimeout(fd, 180)
        socket.set_enable_chunked(fd,"wr")
        print("accept", fd, moon.decode(msg, "Z"))
    end)

    socket.on("message", function(fd, msg)
        local buf = moon.decode(msg, "B")
        ---@type cluster_header
        local header = unpack_one(buf, true)
        if header.ping then
            socket.write(fd, pack({pong = true}))
            return
        elseif header.pong then
            return
        end

        if header.session < 0 then -- receive call message
            moon.async(function ()
                local address = services_address[header.to_sname]
                if not address then
                    local message = string.format("Service named '%s' not found for node '%s'.", header.to_sname, header.to_node)
                    header.session = -header.session
                    socket.write(fd, pack(header, false, message)) -- response to sender node
                    moon.error("An error occurred while receiving the cluster.call message:", message)
                    return
                end
                local session = moon.next_sequence()
                redirect(msg, address, moon.PTYPE_LUA, moon.id, -session)
                header.session = -header.session
                socket.write(fd, pack(header, moon.wait(session, address)))
            end)
        elseif header.session > 0 then --receive response message
            if remove_send_watch(fd, header.from_addr, header.session) then
                redirect(msg, header.from_addr, moon.PTYPE_LUA, moon.id, header.session)
            end
        else-- receive send message
            local address = services_address[header.to_sname]
            if not address then
                local message = string.format("Service named '%s' not found for node '%s'.", header.to_sname, header.to_node)
                moon.error("An error occurred while receiving the cluster.send message:",message)
                return
            end
            redirect(msg, address, moon.PTYPE_LUA)
        end
    end)

    socket.on("close", function(fd, msg)
        print("socket close", moon.decode(msg, "Z"))
        for k, v in pairs(close_watch) do
            if v == fd then
                close_watch[k] = false
            end
        end

        local senders = send_watch[fd]
        if senders then
            for key in pairs(senders) do
                local arr = string.split(key,"-")
                local sessionid = tonumber(arr[1])
                local sender = tonumber(arr[2])
                print("response to sender service", sender, sessionid)
                ---@diagnostic disable-next-line: param-type-mismatch
                moon.response("lua", sender, -sessionid, false, "cluster:socket disconnect")
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

    local function connect(node)
        local c = clusters[node]
        local fd, err = socket.connect(c.host, c.port, moon.PTYPE_SOCKET_MOON, 1000)
        if not fd then
            moon.error(err)
            return
        end
        socket.set_enable_chunked(fd, "wr")
        return fd
    end

    local command = {}

    local lock = require("moon.queue")()

    function command.Listen()

        local response = httpc.get(conf.etc_host,{
            path = string.format(conf.etc_path, NODE)
        })

        if response.status_code ~= 200 then
            error("can not found cluster config for node="..NODE)
        end

        local c = json.decode(response.content)
        assert(c.host and c.port, "require host and port")

        local listenfd = socket.listen(c.host, c.port,moon.PTYPE_SOCKET_MOON)
        socket.start(listenfd)
        print(strfmt("cluster listen %s:%d", c.host, c.port))
        setmetatable(clusters, {__gc=function()
            socket.close(listenfd)
        end})
        return true
    end

    function command.Request(msg)

        local buf = moon.decode(msg, "L")

        ---@type cluster_header
        local header = unpack_one(buf)

		local shr = buffer.to_shared(buf)
        local c = clusters[header.to_node]
        if c and c.fd and socket.write(c.fd, shr) then
            if header.session < 0 then
                --记录mode-call消息，网络断开时，返回错误信息
                add_send_watch(c.fd, header.from_addr, -header.session)
            end
            return
        else
            if c then
                c.fd = false
            end
        end

        local data = buffer.unpack(buf)

        local scope<close> = lock()

        c = clusters[header.to_node]
        if not c or not c.fd then
            local response = httpc.get(conf.etc_host,{
                path = string.format(conf.etc_path, header.to_node)
            })
            if response.status_code ~= 200 then
                local errstr = response.content
                moon.error(response.status_code, errstr)
                moon.response("lua", header.from_addr, header.session, false, errstr)
                return
            end
            c = json.decode(response.content)
            clusters[header.to_node] = c
            c.fd = connect(header.to_node)
        end

        if c.fd and socket.write(c.fd, data) then
            if header.session < 0 then
                --记录mode-call消息，网络断开时，返回错误信息
                add_send_watch(c.fd, header.from_addr, -header.session)
            end
            return
        end

        c.fd = false

        if header.session == 0 then
            moon.error("not connected cluster")
            return
        else
            --CASE1:connect failed, mode-call, 返回错误信息
            moon.response("lua", header.from_addr, header.session, false, "connect failed:"..tostring(header.to_node))
            return
        end
    end

    moon.async(function()
        while true do
            moon.sleep(5000)
            for _,senders in pairs(send_watch) do
                for key, t in pairs(senders) do
                    if moon.time() - t > 10 then
                        local sender = key&0xFFFFFFFF
                        local sessionid = (key>>32)
                        moon.response("lua", sender, -sessionid, false, "cluster:socket read timeout")
                        senders[key] = nil
                    end
                end
            end

            for _, v in pairs(clusters) do
                if v.fd then
                    socket.write(v.fd, pack({ping = true}))
                end
            end
        end
    end)

    local function xpcall_ret(ok, ...)
        if ok then
            return moon.pack(...)
        end
        return moon.pack(false, ...)
    end

    moon.raw_dispatch("lua", function(m)
        moon.async(function(msg)
            local sender, session, buf = moon.decode(msg, "SEB")
            local cmd = unpack_one(buf, true)
            local fn = command[cmd]
            if fn then
                if session ~= 0 then
                    moon.raw_send("lua", sender, xpcall_ret(xpcall(fn, debug.traceback, msg)), session)
                else
                    fn(msg)
                end
            else
                if session == 0 then
                    moon.error(moon.name, "recv unknown cmd "..tostring(cmd))
                else
                    moon.response("lua", sender, session, false, moon.name.." recv unknown cmd "..tostring(cmd))
                end
            end
        end, m)
    end)

    moon.shutdown(function()
        moon.quit()
    end)
end

if conf and conf.name then
    cluster_service()
end

local cluster = {}

local cluster_address

---@class cluster_header
---@field public to_node integer @ 接收者服务器ID
---@field public to_sname string @ 接收者服务name
---@field public from_node integer @ 发送者服务器ID
---@field public from_addr integer @ 发送者服务ID
---@field public session integer @ 协程session
---@field public ping boolean
---@field public pong boolean

---@param receiver_node integer @ node ID
---@param receiver_sname string @ 目标node的 唯一服务 name
---@param ... any @ 参数
function cluster.send(receiver_node, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        assert(cluster_address>0)
    end

    local header = {
        to_node = receiver_node,
        to_sname = receiver_sname,
        from_node = NODE,
        from_addr = moon.id,
        session = 0
    }

    moon.send("lua", cluster_address, "Request", header, ...)
end

---@param receiver_node integer @ node ID
---@param receiver_sname string @ 目标node的 唯一服务 name
---@param ... any @ 参数
function cluster.call(receiver_node, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        assert(cluster_address>0)
    end

    local session = moon.next_sequence()

    local header = {
        to_node = receiver_node,
        to_sname = receiver_sname,
        from_node = NODE,
        from_addr = moon.id,
        session = -session
    }

    moon.raw_send("lua", cluster_address, pack("Request", header, ...))
    return moon.wait(session)
end

return cluster