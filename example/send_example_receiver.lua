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

moon.start(function()

	moon.dispatch('lua',function(msg,p)
		local sender = msg:sender()
        local header = msg:header()
        docmd(sender,header, p.unpack(msg))
    end)
end)

