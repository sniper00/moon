local moon = require("moon")

local command = {}

command.HELLO = function(sender, ...)
	moon.send('lua', sender,'WORLD', ...)
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

