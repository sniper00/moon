package.path = 'Base/?.lua;Gate/?.lua;Login/?.lua;'
local MsgID = require("MsgID")

local mgr = ModuleManager.new()

mgr:Init("machine_id:1;worker_num:1;")

mgr:CreateModule("name:gate;luafile:Gate/Gate.lua")
mgr:CreateModule("name:login;luafile:Login/Login.lua")

local msg  = CreateMessage()
msg:SetType(EMessageType.ModuleData)

local mr   = MessageWriter.new(msg)
mr:WriteUint16(MsgID.MSG_S2S_SERVER_START)

mgr:Broadcast(0,msg)

mgr:Run()

local name = io.read()

mgr:Stop()
