local moon = require("moon")

local handler = {1, 2, 3, 4, 5, 6}

moon.dispatch(
    "socket",
    function(msg)
        local sessionid = msg:sender()
        local subtype = msg:subtype()
        local f = handler[subtype]
        if f then
            f(sessionid, msg)
        end
    end
)

local M = {}

local tcp = moon.get_tcp()

local type_map = {
    connect = 1,
    accept = 2,
    message = 3,
    close = 4,
    error = 5
}

function M.connect( ... )
    return tcp:connect(...)
end

function M.settimeout( ... )
    tcp:settimeout(...)
end

function M.on(name, cb)
    local n = type_map[name]
    if n then
        handler[n] = cb
    end
end

function M.send_message(sessionid, msg)
    return tcp:send_message(sessionid, msg)
end

function M.send(sessionid, str)
    return tcp:send(sessionid, str)
end

function M.send_then_close(sessionid, str)
    return tcp:send_then_close(sessionid, str)
end

function M.set_enable_frame(flag)
    tcp:set_enable_frame(flag)
end

function M.close(sessionid)
    return tcp:close(sessionid)
end

return M
