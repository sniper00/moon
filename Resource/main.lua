package.path = 'Base/?.lua;Gate/?.lua;Login/?.lua;'
local MsgID = require("MsgID")

local mgr = ModuleManager.new()

mgr:Init("machine_id:1;worker_num:2;")

mgr:CreateModule("name:gate;luafile:Gate/Gate.lua")
mgr:CreateModule("name:login;luafile:Login/Login.lua")

local msg  = CreateMessage()
msg:SetType(EMessageType.ModuleData)
local bw = MessageWriter.new(msg)
bw:WriteUint16(MsgID.MSG_S2S_SERVER_START)

mgr:BroadcastMessage(0,msg)

msg = nil

mgr:Run()

local name = io.read()

mgr:Stop()
