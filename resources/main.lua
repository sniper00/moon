package.path = 'Base/?.lua;Common/?.lua;'

--注册 lua_service
lua_service.register("lua")
--register service end

--init service pool machineid 为1，worker线程数 2
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

