require("functions")

local Component 	= require("Component")
local Stream		        = require("Stream")
local BinaryReader   = require("BinaryReader")
local MsgID             = require("MsgID")

local  MonitorHandler 	= class("MonitorHandler",Component)

function MonitorHandler:ctor()
	MonitorHandler.super.ctor(self)

	thisService:RegisterMessageCB(MsgID.MSG_S2S_SERVER_START, handler(self,self.OnServerStart))
	thisService:RegisterMessageCB(MsgID.MSG_S2S_MODULE_START, handler(self,self.OnServiceStart))

	Log.Trace("ctor MonitorHandler")
end

function MonitorHandler:OnServerStart(uctx,data,rpc)
	Log.ConsoleTrace("Monitor Service: server start")

end

function MonitorHandler:OnServiceStart(uctx,data,rpc)
	local br   = BinaryReader.new(data)
	local name = br:ReadString()
	local id   = br:ReadUInt32()

	if id ~= thisService:GetID() then
		Log.ConsoleTrace("Monitor: %s %d module start",name,id)
		thisService:RegisterOtherService(name,id)
	end
end

return MonitorHandler

