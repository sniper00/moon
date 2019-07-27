local moon = require("moon")

local command = {}

command.PING = function(sender, ...)
    print(moon.name(), "recv ", sender, "command", "PING")
    print(moon.name(), "send to", sender, "command", "PONG")
	moon.send('lua', sender,'PONG', ...)
end

local function docmd(sender,header,...)
	-- body
	local f = command[header]
	if f then
		f(sender,...)
	else
		error(string.format("Unknown command %s", tostring(header)))
	end
end

moon.dispatch('lua',function(msg,p)
	local sender = msg:sender()
	local header = msg:header()
	docmd(sender,header, p.unpack(msg))
end)

