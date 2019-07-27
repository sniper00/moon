local moon = require("moon")

local command = {}

command.PING = function(sender,sessionid, ...)
    print(moon.name(), "recv ", sender, "command", "PING")
    print(moon.name(), "send to", sender, "command", "PONG")
    -- 把sessionid发送回去，发送方resume对应的协程
    moon.response("lua",sender,sessionid,'PONG', ...)
end

local function docmd(sender,sessionid,cmd,...)
	-- body
	local f = command[cmd]
	if f then
		f(sender,sessionid,...)
	else
		error(string.format("Unknown command %s", tostring(cmd)))
	end
end

moon.dispatch('lua',function(msg,p)
    local sender = msg:sender()
    -- sessionid 对应表示发送方 挂起的协程
    local sessionid = msg:sessionid()
	docmd(sender,sessionid, p.unpack(msg))
end)

