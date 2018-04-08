local moon = require("moon")

--Echo Server 示例

local network = moon.get_component_tcp("network")
local socket_handler = {}

-- enum class socket_data_type :std::uint8_t
-- {
--     socket_connect = 1,
--     socket_accept =2,
--     socket_recv = 3,
--     socket_close =4,
--     socket_error = 5,
--     socket_logic_error = 6
-- };

socket_handler[1] = function(sessionid, data)
	print("connect ",sessionid,data)
end

socket_handler[2] = function(sessionid, data)
	print("accept ",sessionid,data)
end

socket_handler[3] = function(sessionid, data)
	network:send(sessionid, data)
	print("recv", data)
end

socket_handler[4] = function(sessionid, data)
	print("close ",sessionid, data)
end

socket_handler[5] = function(sessionid, data)
	print("error ",sessionid, data)
end

socket_handler[6] = function(sessionid, data)
	print("logic error ",sessionid, data)
end

moon.start(function()
	moon.register_protocol(
	{
		name = "socket",
		PTYPE = moon.PSOCKET,
		pack = function(...) return ... end,
		dispatch = function(msg)
			local sessionid = msg:sender()
			local subtype = msg:subtype()
			local f = socket_handler[subtype]
			if f then
				local data = msg:bytes()
				f(sessionid, data)
			end
		end
	})
end)


