package.path = 'Base/?.lua;Gate/?.lua;Login/?.lua;'

require("SerializeUtil")

local MsgID 	= require("MsgID")

local mgr = ModuleManager.new()
mgr:Init("machine_id:1;worker_num:1;")
mgr:CreateModule(
	[[
	name:gate;
	luafile:Gate/Gate.lua;
	netthread:1;
	ip:127.0.0.1;
	port:11111;
	]])

mgr:CreateModule(
	[[
	name:login;
	luafile:Login/Login.lua;
	]])

mgr:Broadcast(0,UInt16ToBytes(MsgID.MSG_S2S_SERVER_START),"",EMessageType.ModuleData)

mgr:Run()

local name = io.read()

mgr:Stop()
