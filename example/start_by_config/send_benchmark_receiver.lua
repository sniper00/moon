local moon = require("moon")

local command = {}

command.TEST = function(sender, ...)
	moon.send('lua', sender, 'TEST', ...)
end

local function docmd(sender, cmd, ...)
	-- body
	local f = command[cmd]
	if f then
		f(sender,...)
	else
		error(string.format("Unknown command %s", tostring(cmd)))
	end
end

moon.dispatch('lua',function(msg,unpack)
	local sender, p, n = moon.decode(msg, "SC")
	docmd(sender, unpack(p, n))
end)

