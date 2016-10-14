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

	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SERVER_START, handler(self,self.OnServerStart))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_MODULE_START, handler(self,self.OnModuleStart))

	Log.Trace("ctor GateHandler")
end

function GateHandler:OnServerStart(uctx,data,rpc)
	Log.ConsoleTrace("Gate Module: server start")

	local s = Stream.new()
	s:WriteUInt16(MsgID.MSG_S2S_MODULE_START)
	s:WriteString(thisModule:GetName())
	s:WriteUInt32(thisModule:GetID())

	thisModule:Broadcast(s:Bytes())
end

function GateHandler:OnModuleStart(uctx,data,rpc)
	local br   = BinaryReader.new(data)
	local name = br:ReadString()
	local id   = br:ReadUInt32()

	if id ~= thisModule:GetID() then
		Log.ConsoleTrace("Gate: %s %d module start",name,id)
		thisModule:RegisterOtherModule(name,id)
		if name == "world" then
			thisModule:SetWorldModule(id)
		elseif name == "login" then
			thisModule:SetLoginModule(id)
		end
	end

end

return GateHandler

