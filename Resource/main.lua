package.path = 'Base/?.lua;Gate/?.lua;Login/?.lua;'

require("SerializeUtil")
require("gate.c")

local MsgID = require("MsgID")

pool:init("machineid:1;service_worker_num:2;")

pool:run()

-- local gt = Gate.Create()
-- pool:new_native_service(gt,
-- 	[[
-- 	name:gate;
-- 	netthread:4;
-- 	ip:127.0.0.1;
-- 	port:11111;
-- 	]],true)

pool:new_service(
	[[
	name:gate;
	luafile:Gate/Gate.lua;
	netthread:4;
	timeout:0;
	ip:127.0.0.1;
	port:11111;
	]],true)

pool:new_service(
	[[
	name:login;
	luafile:Login/Login.lua;
	]],true)

pool:new_service(
	[[
	name:monitor;
	luafile:Monitor/Monitor.lua;
	]],false)

pool:broadcast(0,UInt16ToBytes(MsgID.MSG_S2S_SERVER_START))

local name = io.read()


pool:stop()
