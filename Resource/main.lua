package.path = 'Base/?.lua;Gate/?.lua;Login/?.lua;'
local MsgID = require("MsgID")
local Stream = require("Stream")

local mgr = ModuleManager.new()
mgr:Init("machine_id:1;worker_num:1;")
mgr:CreateModule("name:gate;luafile:Gate/Gate.lua")
mgr:CreateModule("name:login;luafile:Login/Login.lua")

local msg =  CreateMessage()
msg:WriteData(UIn16ToBytes(MsgID.MSG_S2S_SERVER_START))
msg:SetType(EMessageType.ModuleData)

mgr:Broadcast(0,msg)

mgr:Run()

local name = io.read()

mgr:Stop()
