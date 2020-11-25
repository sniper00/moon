local moon = require("moon")

local command = {}

command.TEST = function(sender, ...)
	moon.send('lua', sender,'TEST', ...)
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

moon.dispatch('lua',function(msg,unpack)
	local sender, header, p, n = moon.decode(msg, "SHC")
	docmd(sender,header, unpack(p, n))
end)

