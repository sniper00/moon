local moon = require("moon")

local close_flag = moon.buffer_flag.close

local text_flag = moon.buffer_flag.close

local handler = table.new(0,6)

moon.dispatch(
    "websocket",
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

local tcp = moon.get_tcp('websocket')

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

--- send binary
function M.send_message(sessionid, msg)
    return tcp:send_message(sessionid, msg)
end

--- send binary
function M.send(sessionid, str)
    return tcp:send(sessionid, str)
end

--- send text
function M.send_text(sessionid, str)
    return tcp:send_with_flag(sessionid, str, text_flag)
end

function M.send_then_close(sessionid, str)
    return tcp:send_with_flag(sessionid, str, close_flag)
end

function M.close(sessionid)
    return tcp:close(sessionid)
end

return M
