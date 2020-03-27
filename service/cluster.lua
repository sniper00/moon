local moon = require("moon")
local seri = require("seri")

local conf = ...

local pack = seri.pack
local packs = seri.packs
local unpack = seri.unpack
local co_yield = coroutine.yield

local function cluster_service()

    local json = require("json")
    local buffer = require("buffer")
    local socket = require("moon.socket")

    local clusters = {}
    local print = print
    local assert = assert
    local pairs = pairs
    local strfmt = string.format

    local unpack_one = seri.unpack_one
    local write_back = buffer.write_back
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
        --print("addkey", key, sessionid)
        senders[key] = true
    end

    local function remove_send_watch( fd, sender, sessionid )
        local senders = send_watch[fd]
        if not senders then
            assert(false)
            return
        end

        local key = ((-sessionid)<<32)|sender
        --print("remove key", key, sessionid)
        senders[key] = nil
    end

    local services_address = {}
    local function get_service_address(sname)
        local addr = services_address[sname]
        if not addr then
            addr = queryservice(sname)
            assert(addr>0, sname)
            services_address[sname] = addr
        end
        return addr
    end

    local function unpack_header(msg)
        local buf = msg:buffer()
        local receiver_sname = unpack_one(buf,true)
        local sender_addr = unpack_one(buf,true)
        local sessionid = unpack_one(buf,true)
        return receiver_sname, sender_addr, sessionid
    end

    local function do_call(fd, msg, receiver_sname, sender_addr, sessionid)
        local address = get_service_address(receiver_sname)
        assert(address>0, receiver_sname)

        local sessionid_ = moon.make_response(address)
        moon.redirect(msg, "", address, moon.PTYPE_LUA, moon.sid(), -sessionid_)

        close_watch[sessionid_] = fd
        local resmsg, err = co_yield()
        local state = close_watch[sessionid_]
        close_watch[sessionid_] = nil
        if state == false then
            return
        end

        if resmsg then
            local unsafe_buf = pack(receiver_sname, sender_addr, -sessionid)
            write_back(unsafe_buf, resmsg:bytes())
            socket.write(fd, unsafe_buf)
        else
            moon.error(err)
            local res = packs(receiver_sname, sender_addr, -sessionid, false, err)
            socket.write(fd, res)
        end
    end

    socket.on("connect",function(fd, msg)
        print("connect", fd, msg:bytes())
    end)

    socket.on("accept",function(fd, msg)
        socket.settimeout(fd, 180)
        print("accept", fd, msg:bytes())
    end)

    socket.on("message", function(fd, msg)
        local receiver_sname, sender_addr, sessionid = unpack_header(msg)
        if sessionid < 0 then
            moon.async(function ()
                local ok, err = pcall(do_call, fd, msg, receiver_sname, sender_addr, sessionid)
                if not ok then
                    local res = packs(receiver_sname,
                                            sender_addr,
                                            -sessionid,
                                            false,
                                            err)
                    socket.write(fd, res)
                end
            end)
        elseif sessionid > 0 then
            remove_send_watch(fd, sender_addr, -sessionid)
            moon.redirect(msg, "", sender_addr, moon.PTYPE_LUA, moon.sid(), sessionid)
        else-- receive send message
            local address = get_service_address(receiver_sname)
            moon.redirect(msg, "", address, moon.PTYPE_LUA)
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
            for key in pairs(senders) do
                local sender = key&0xFFFFFFFF
                local sessionid = -(key>>32)
                print("response", sender, sessionid)
                moon.response("lua", sender, sessionid, packs(false, "connect closed"))
            end
        end

        for k,v in pairs(connectors) do
            if v == fd then
                connectors[k] = nil
                clusters[k].state = "CLOSED"
            end
        end
    end)

    socket.on("error", function(fd, msg)
        print("socket error",fd, msg:bytes())
    end)

    local function connect(serverid)
        local c = clusters[serverid]
        c.state = "CONNECTING"
        local fd, err = socket.connect(c.host, c.port, moon.PTYPE_SOCKET)
        if not fd then
            moon.error(err)
            c.state = "CLOSED"
            return
        end
        socket.set_enable_chunked(fd, "wr")
        c.state = "CONNECTED"
        return fd
    end

    local message_cache = {}

    local function get_message_cache(serverid)
        local cache = message_cache[serverid]
        if not cache then
            cache = {}
            message_cache[serverid] = cache
        end
        return cache
    end

    local command = {}

    function command.SEND(serverid, msg)
        local sender = msg:sender()
        local sessionid = msg:sessionid()
        local fd = connectors[serverid]

        if fd then
            if sessionid < 0 then
                add_send_watch(fd, sender, sessionid)
            end
            socket.write_message(fd, msg)
            return
        end

        local data = msg:bytes()
        local cache = get_message_cache(serverid)
        table.insert(cache, {sender, sessionid, data})

        local state = clusters[serverid].state
        if state ~= "CLOSED" then
            return
        end

        moon.async(function ()
            fd = connect(serverid)
            message_cache[serverid] = nil
            if not fd then
                for _, v in ipairs(cache) do
                    if v[2] ~=0 then
                        moon.response("lua", v[1], v[2], seri.packs(false, "not connected"))
                    else
                        moon.error("can not connect cluster node",
                            serverid, "drop message from", sender, "size", #v[3])
                    end
                end
            else
                for _, v in ipairs(cache) do
                    if v[2] ~=0 then
                        add_send_watch(fd, v[1], v[2])
                    end
                    socket.write(fd, v[3])
                end
                connectors[serverid] = fd
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
                clusters[c.sid]={host = host,port = port, state="CLOSED"}
            end
        end
    end

    load_config()

    local SERVERID = math.tointeger(moon.get_env("SID"))
    if not clusters[SERVERID] then
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
            local cmd, receiver_serverid = unpack(msg:header())
            local fn = command[cmd]
            if fn then
                fn(receiver_serverid, msg)
            else
                moon.error(moon.name(), "recv unknown cmd "..cmd)
            end
        end
    })

    moon.start(function()
        local listenfd = socket.listen(conf.host, conf.port,moon.PTYPE_SOCKET)

        socket.start(listenfd)

        print(strfmt("cluster run at %s %d", conf.host, conf.port))
    end)
end

if conf and conf.name then
    cluster_service()
end

local cluster = {}

local cluster_address

function cluster.send(receiver_serverid, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        if 0 == cluster_address then
            return false, "clusterd service not start"
        end
    end

    moon.raw_send("lua", cluster_address, packs("SEND",receiver_serverid) ,
                    pack(receiver_sname, moon.sid(), 0,...), 0)
end

function cluster.call(receiver_serverid, receiver_sname, ...)
    if not cluster_address then
        cluster_address = moon.queryservice("cluster")
        if 0 == cluster_address then
            return false, "clusterd service not start"
        end
    end

    local sessionid = moon.make_response(cluster_address)
    moon.raw_send("lua", cluster_address, packs("SEND",receiver_serverid) ,
                    pack(receiver_sname, moon.sid(), -sessionid,...), sessionid)
    return co_yield()
end

return cluster