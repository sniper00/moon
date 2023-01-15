local moon = require("moon")
local seri = require("seri")
local socket = require("moon.socket")

local conf = ...

local redirect = moon.redirect

---@class gate_context
local context = {
    conf = conf,
    uid_map = {},
    fd_map = {},
    auth_watch = {},
    ---other service address
    addr_auth = 0,
}

socket.on("accept", function(fd, msg)
    print("GAME SERVER: accept ", fd, moon.decode(msg, "Z"))
    socket.set_enable_chunked(fd, "w")
    --socket.settimeout(fd, 60)
end)

socket.on("message", function(fd, msg)
    -- local c = context.fd_map[fd]
    -- if not c then
    --     ---first message must be auth message
    --     context.auth_watch[fd] = tostring(msg)
    --     local name, req = protocol.decode(moon.decode(msg,"B"))
    --     req.sign = context.auth_watch[fd]
    --     req.fd = fd
    --     req.addr = socket.getaddress(fd)
    --     moon.send("lua", context.addr_auth, name, req)
    -- else
    --     if moon.DEBUG() then
    --         protocol.print_message(c.uid, msg)
    --     end
    --     redirect(msg, "", c.addr_user, PTYPE_C2S, 0, 0)
    -- end
end)

socket.on("close", function(fd, msg)
    local data = moon.decode(msg, "Z")
    context.auth_watch[fd] = nil
    local c = context.fd_map[fd]
    if not c then
        print("GAME SERVER: close", fd, data)
        return
    end
    context.fd_map[fd] = nil
    context.uid_map[c.uid] = nil
    moon.send('lua', context.addr_auth, "Auth.Disconnect", c.uid)
    print("GAME SERVER: close", fd, c.uid, data)
end)

moon.register_protocol({
    name = "S2C",
    PTYPE = 101,
    dispatch = nil
})

moon.register_protocol({
    name = "SBC",
    PTYPE = 102,
    dispatch = nil
})

--- server send to client
moon.raw_dispatch("S2C",function(msg)
    ---@cast msg message_ptr
    local uid = seri.unpack(moon.decode(msg, "H"))
    local c = context.uid_map[uid]
    if not c then
        return
    end
    socket.write_message(c.fd,msg)
end)

-- server broadcast to client
moon.raw_dispatch("SBC",function(msg)
    ---@cast msg message_ptr
    for _, c in pairs(context.uid_map) do
        socket.write_message(c.fd, msg)
    end
end)



