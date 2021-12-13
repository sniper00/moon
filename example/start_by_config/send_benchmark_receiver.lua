local moon = require("moon")

local command = {}

command.TEST = function(sender, ...)
	moon.send('lua', sender, 'TEST', ...)
end

moon.dispatch('lua',function(sender, session, cmd, ...)
	-- body
	local f = command[cmd]
	if f then
		f(sender,...)
	else
		error(string.format("Unknown command %s", tostring(cmd)))
	end
end)

