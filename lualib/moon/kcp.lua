local moon = require("moon")
local socket = require("moon.socket")
local core = require("kcp.core")

local debug = debug

local coroutine = coroutine

local connections = {}

local read_session = {}

local seq = 1

local function update_read(obj)
    while true do
        local session, data = core.poll_read(obj.kcp)
        if session then
            local co = read_session[session]
            read_session[session] = nil
            obj.session = 0
            local ok, err = xpcall(coroutine.resume, debug.traceback, co, data)
            if not ok then
                moon.error(err)
            end
        else
            break
        end
    end
end

local function make_connect(endpoint, close_callback, conv, fd, isclient)
    if not conv then
        conv = seq
        seq = seq + 1
        if seq == 0x7FFFFFFF then
            seq = 1
        end
    end
    local obj = {
        fd = fd,
        addr = endpoint,
        conv = conv,
        recvtime = moon.clock(),
        kcp = core.create(conv),
        closed = false,
        session = 0,
        isclient = isclient,
        nextupdate = 0,
        close_callback = close_callback
    }

    if isclient then
        connections[fd] = obj
    else
        connections[endpoint] = obj
    end
    return obj
end

local function update()
    moon.async(function(...)
        while true do
            for _, obj in pairs(connections) do
                if not obj.closed then
                    local tm = moon.clock()
                    if tm - obj.recvtime > 10 then
                        obj.closed = true
                        core.release(obj.kcp)
                        obj.kcp = nil
                        if obj.isclient then
                            connections[obj.fd] = nil
                        else
                            connections[obj.addr] = nil
                        end
                        if obj.close_callback then
                            obj.close_callback(moon.escape_print(obj.addr))
                        end
                        if obj.session then
                            local co = read_session[obj.session]
                            if co then
                                print("timeout", tm, obj.recvtime, obj.isclient and "client" or "server")
                                coroutine.resume(co, false, "close")
                            end
                        end
                    else
                        update_read(obj)
                        if obj.nextupdate <= moon.clock() * 1000 then
                            obj.nextupdate = core.update(obj.kcp)
                            while true do
                                local sdata = core.output(obj.kcp)
                                if not sdata then
                                    break
                                end
                                if obj.isclient then
                                    socket.write(obj.fd, sdata)
                                else
                                    socket.sendto(obj.fd, obj.addr, sdata)
                                end
                            end
                        end
                    end
                end
            end
            moon.sleep(10)
        end
    end)
end
update()


local kcp = {}

function kcp.listen(host, port, connect_callback, close_callback)
    local fd
    fd = socket.udp(function(str, endpoint)
        local obj = connections[endpoint]
        if #str < 24 and str == "SYN" then
            --print("syn", endpoint)
            obj = obj or make_connect(endpoint, close_callback, nil, fd, false)
            obj.recvtime = moon.clock()
            socket.sendto(fd, endpoint, "ACK" .. string.pack(">I", obj.conv))
            connect_callback(endpoint)
            return
        end

        if not obj then
            return
        end

        obj.recvtime = moon.clock()
        core.input(obj.kcp, str)
        update_read(obj)
    end, host, port)
end

function kcp.connect(host, port, timeout)
    local co = coroutine.running()
    local istimeout = false
    local timerid
    if timeout then
        timerid = moon.timeout(timeout, function()
            istimeout = true
            coroutine.resume(co, false, "timeout")
        end)
    end
    local fd
    fd = socket.udp(function(str, endpoint)
        local obj = connections[fd]
        if #str < 24 and string.sub(str, 1, 3) == "ACK" then
            if istimeout then return end
            --print("ack", moon.md5(endpoint), fd)
            local conv = string.unpack(">I", str, 4)
            obj = make_connect(endpoint, nil, conv, fd, true)
            if timerid then
                moon.remove_timer(timerid)
            end
            coroutine.resume(co, fd)
            return
        end
        if not obj then
            return
        end
        obj.recvtime = moon.clock()
        core.input(obj.kcp, str)
        update_read(obj)
    end)

    socket.udp_connect(fd, host, port)
    socket.write(fd, "SYN")
    return coroutine.yield()
end

function kcp.send(endpoint, data)
    local obj = connections[endpoint]
    if obj and not obj.closed then
        return core.send(obj.kcp, data)
    end
    return false
end

local read_seq = 1
---async
function kcp.read(endpoint, len)
    assert(len > 0)
    local obj = connections[endpoint]
    if obj and not obj.closed then
        assert(obj.session == 0)
        local session, data = core.poll_read(obj.kcp)
        if session then
            return data
        end
        obj.session = read_seq
        read_seq = read_seq + 1
        if read_seq == 0x7FFFFFFF then
            read_seq = 1
        end
        core.read(obj.kcp, obj.session, len)
        read_session[obj.session] = coroutine.running()
        return coroutine.yield()
    end
    return false
end

return kcp
