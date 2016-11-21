require("functions")
require("SerializeUtil")

local MsgID 		= require("MsgID")
local Component 	= require("Component")
local Stream		= require("Stream")
local BinaryReader = require("BinaryReader")


local  GateHandler 	= class("GateHandler",Component)

function GateHandler:ctor()
	GateHandler.super.ctor(self)
	GateHandler.super.SetEnableUpdate(self,true)

	thisService:RegisterMessageCB(MsgID.MSG_S2S_SERVER_START, handler(self,self.OnServerStart))
	thisService:RegisterMessageCB(MsgID.MSG_S2S_MODULE_START, handler(self,self.OnServiceStart))

	Log.Trace("ctor GateHandler")
end

function GateHandler:Start()
	local s = Stream.new()
	s:WriteUInt16(MsgID.MSG_S2S_MODULE_START)
	s:WriteString(thisService:GetName())
	s:WriteUInt32(thisService:GetID())
	thisService:Broadcast(s:Bytes())
end

function GateHandler:OnServerStart(uctx,data,rpc)
	Log.ConsoleTrace("Gate Service: server start")
end

function GateHandler:OnServiceStart(uctx,data,rpc)
	local br   = BinaryReader.new(data)
	local name = br:ReadString()
	local id   = br:ReadUInt32()

	if id ~= thisService:GetID() then
		Log.ConsoleTrace("Gate: %s %d module start",name,id)
		thisService:RegisterOtherService(name,id)
		if name == "world" then
			thisService:SetWorldService(id)
		elseif name == "login" then
			thisService:SetLoginService(id)
		end
	end

end

return GateHandler

