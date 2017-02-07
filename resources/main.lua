package.path = 'Base/?.lua;Common/?.lua;'

local function register_service(s,service_type)
	assert(type(s)=='string' and type(service_type)=='string')

	local script
	if s == "lua_service" then
		script = string.format([[
			%s.register("%s")
		]],s,service_type)
	else
		script = string.format([[
			require("%s")
			%s.register("%s")
		]],s,s,service_type)
	end
	local f = load(script)
	f()
end

--注册 lua_service
register_service("lua_service","lua")
--register service end

--init service pool machineid 为1，worker线程数 1
pool:init(1,1)

pool:run()

pool:new_service("lua",[[
{
	"name":"EchoServer",
	"file":"EchoServer.lua",
	"network":{
		"type":"listen",
		"thread":4,
		"timeout":30,
		"ip":"127.0.0.1",
		"port":11111
	}
}
]], true)

--block main thread, avoid exit, until input 'exit'
local name = io.read()
while name ~= 'exit' do
    name = io.read()
end

pool:stop()

