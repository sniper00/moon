local moon   = require("moon")
local seri   = require("seri")
local buffer = require("buffer")

local conf   = ...

local pack   = seri.pack

local NODE   = math.tointeger(moon.env("NODE"))

local function cluster_service()
    local json = require("json")
    local socket = require("moon.socket")
    local httpc = require("moon.http.client")
    local co_mutex = require("moon.queue")

    local print = print
    local assert = assert
    local pairs = pairs
    local strfmt = string.format

    local unpack_one = seri.unpack_one

    local redirect = moon.redirect

    ---@type table<integer, integer>
    local clusters = {}
    local call_watch = {}
    local connect_mutex = setmetatable({}, { __mode = "v" })

    local function find_or_create_connection(node)
        local lock = connect_mutex[node]
        if not lock then
            lock = co_mutex()
            connect_mutex[node] = lock
        end

        local scope <close> = lock()

        local fd = clusters[node]
        if not fd then
            local response = httpc.get(string.format(conf.url, node))
            if response.status_code ~= 200 then
                local errstr = response.body
                -- moon.error(string.format("find_or_create_connection: failed to fetch cluster config for node=%s url=%s status=%s body=%s", tostring(node), tostring(string.format(conf.url, node)), tostring(response.status_code), tostring(errstr)))
                return nil, errstr
            end
            ---@diagnostic disable-next-line: assign-type-mismatch
            ---@type {host:string, port:integer}
            local addr = json.decode(response.body)
            local err
            ---@diagnostic disable-next-line: await-in-sync
            fd, err = socket.connect(addr.host, addr.port, moon.PTYPE_SOCKET_MOON, 1000)
            if not fd then
                -- moon.error(strfmt("find_or_create_connection: connect to addr failed for node=%s addr=%s:%s err=%s", node, addr.host, addr.port, err))
                return nil, err
            end
            socket.set_enable_chunked(fd, "wr")
            clusters[node] = fd
        end
        return fd
    end

    local function add_call_watch(fd, sender, sessionid)
        local token = sessionid .. "-" .. sender
        if call_watch[token] then
            error(strfmt("add_call_watch: duplicate call_watch fd=%s sender=%s sessionid=%s", fd, sender, sessionid))
        end
        call_watch[token] = { time = moon.time(), fd = fd }
    end

    local function remove_call_watch(sender, sessionid)
        local token = sessionid .. "-" .. sender
        local t = call_watch[token]
        if not t then
            moon.error(strfmt("remove_call_watch: call_watch not found sender=%s sessionid=%s", sender, sessionid))
            return false
        end
        call_watch[token] = nil
        return true
    end

    local services_address = setmetatable({}, {
        __index = function(t, key)
            local addr = moon.queryservice(key)
            if addr == 0 then
                return nil
            end
            t[key] = addr
            return addr
        end
    })

    socket.on("connect", function(fd, msg)
        print("connect", fd, moon.decode(msg, "Z"))
        socket.set_enable_chunked(fd, "wr")
    end)

    socket.on("accept", function(fd, msg)
        socket.settimeout(fd, 180)
        socket.set_enable_chunked(fd, "wr")
        print("accept", fd, moon.decode(msg, "Z"))
    end)

    socket.on("message", function(fd, msg)
        local buf = moon.decode(msg, "B")
        ---@type cluster_header
        local header = unpack_one(buf, true)
        if header.ping then
            socket.write(fd, pack({ pong = true }))
            return
        elseif header.pong then
            return
        end

        if header.session < 0 then -- receive call message
            moon.async(function()
                local address = services_address[header.to_sname]
                if not address then
                    local message = strfmt("cluster.call: service not found to_sname=%s to_node=%s", header.to_sname, header.to_node)
                    header.session = -header.session
                    socket.write(fd, pack(header, false, message)) -- response to sender node
                    moon.error(message)
                    return
                end
                local session = moon.next_sequence()
                redirect(msg, address, moon.PTYPE_LUA, moon.id, -session)
                header.session = -header.session
                local res = buffer.to_shared(pack(header, moon.wait(session)))
                local fd2, err = find_or_create_connection(header.from_node)
                if not fd2 then
                    local message = strfmt("cluster.call: failed to connect back to from_node=%s from_addr=%s to_sname=%s err=%s", header.from_node, header.from_addr, header.to_sname, err)
                    socket.write(fd, pack(header, false, message)) -- response to sender node
                    moon.error(message)
                    return
                end
                socket.write(fd2, res) -- response to sender node
            end)
        elseif header.session > 0 then  --receive response message
            if remove_call_watch(header.from_addr, header.session) then
                redirect(msg, header.from_addr, moon.PTYPE_LUA, moon.id, header.session)
            end
        else -- receive send message
            local address = services_address[header.to_sname]
            if not address then
                moon.error(strfmt("cluster.send: service not found to_sname=%s to_node=%s", header.to_sname, header.to_node))
                return
            end
            redirect(msg, address, moon.PTYPE_LUA)
        end
    end)

    socket.on("close", function(fd, msg)
        print("socket close", moon.decode(msg, "Z"))

        for key, value in pairs(call_watch) do
            if value.fd == fd then
                local arr = string.split(key, "-")
                local sessionid = tonumber(arr[1])
                local sender = tonumber(arr[2])
                moon.warn(strfmt("cluster connection closed, return error to caller sender=%s sessionid=%s", sender, sessionid))
                ---@diagnostic disable-next-line: param-type-mismatch
                moon.response("lua", sender, -sessionid, false, "Cluster call connect closed")
                call_watch[key] = nil
            end
        end

        for node, fd_ in pairs(clusters) do
            if fd_ == fd then
                clusters[node] = nil
                print("remove cluster connection fd=", fd)
                break
            end
        end
    end)

    local command = {}

    function command.Listen()
        local response = httpc.get(string.format(conf.url, NODE))
        if response.status_code ~= 200 then
            error(strfmt("can not found cluster config for node=%s", NODE))
        end

        local c = json.decode(response.body)
        assert(c.host and c.port, "require host and port")

        local listenfd = socket.listen(c.host, c.port, moon.PTYPE_SOCKET_MOON)
        socket.start(listenfd)
        print(strfmt("cluster listen %s:%d", c.host, c.port))
        setmetatable(clusters, {
            __gc = function()
                socket.close(listenfd)
            end
        })
        return true
    end

    function command.Request(msg)
        local buf = moon.decode(msg, "L")

        ---@type cluster_header
        local header = unpack_one(buf)

        local shr = buffer.to_shared(buf)
        local fd = clusters[header.to_node]
        if fd and socket.write(fd, shr) then
            if header.session < 0 then
                --记录mode-call消息，网络断开时，返回错误信息
                add_call_watch(fd, header.from_addr, -header.session)
            end
            return
        else
            clusters[header.to_node] = nil
        end

        local err
        fd, err = find_or_create_connection(header.to_node)
        if not fd then
            moon.error(strfmt("command.Request: create cluster connection failed to_node=%s err=%s", header.to_node, err))
            if header.session == 0 then
                return
            else
                --CASE1:connect failed, mode-call, 返回错误信息
                moon.response("lua", header.from_addr, header.session, false,
                    strfmt("connect failed: to_node=%s err=%s", header.to_node, err))
                return
            end
        end

        if socket.write(fd, shr) then
            if header.session < 0 then
                --记录mode-call消息，网络断开时，返回错误信息
                add_call_watch(fd, header.from_addr, -header.session)
            end
            return
        end

        clusters[header.to_node] = nil

        if header.session == 0 then
            moon.error(strfmt("command.Request: socket write failed to_node=%s", header.to_node))
            return
        else
            --CASE1:connect failed, mode-call, 返回错误信息
            moon.response("lua", header.from_addr, header.session, false, strfmt("socket write failed: to_node=%s", header.to_node))
            return
        end
    end

    moon.async(function()
        while true do
            moon.sleep(5000)
            for k, v in pairs(call_watch) do
                if moon.time() - v.time > 10 then
                    local arr = string.split(k, "-")
                    local sessionid = tonumber(arr[1])
                    local sender = tonumber(arr[2])
                    ---@diagnostic disable-next-line: param-type-mismatch
                    moon.response("lua", sender, -sessionid, false, "Cluster call request timeout")
                    call_watch[k] = nil
                end
            end

            for _, fd in pairs(clusters) do
                socket.write(fd, pack({ ping = true }))
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
                    moon.error(strfmt("%s recv unknown cmd %s", moon.name, cmd))
                else
                    moon.response("lua", sender, session, false, strfmt("%s recv unknown cmd %s", moon.name, cmd))
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
---@field to_node integer @ 接收者服务器ID
---@field to_sname string @ 接收者服务name
---@field from_node integer @ 发送者服务器ID
---@field from_addr integer @ 发送者服务ID
---@field session integer @ 协程session
---@field ping boolean
---@field pong boolean
---@field caller_fd integer

---@param receiver_node integer @ node ID
---@param receiver_sname string @ 目标node的 唯一服务 name
---@param ... any @ 参数
function cluster.send(receiver_node, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        assert(cluster_address > 0)
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
        assert(cluster_address > 0)
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
