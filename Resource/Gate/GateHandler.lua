require("functions")

local SerializeUtil = require("SerializeUtil")
local MsgID 		= require("MsgID")
local Component 	= require("Component")


local  GateHandler 	= class("GateHandler",Component)

function GateHandler:ctor()
	GateHandler.super.ctor(self)

	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SERVER_START, handler(self,self.OnServerStart))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_MODULE_START, handler(self,self.OnModuleStart))
	-- body
	Log.Trace("ctor GateHandler")
end

function GateHandler:OnServerStart(userCtx, data)
	Log.ConsoleTrace("Gate Module: server start")
	local smsg,mw = SerializeUtil.SerializeEx(MsgID.MSG_S2S_MODULE_START)
	mw:WriteString(thisModule:GetName())
	mw:WriteUint32(thisModule:GetID())
	thisModule:Broadcast(smsg)
end

function GateHandler:OnModuleStart(userCtx, data)
	local name = data:ReadString()
	local id   = data:ReadUint32()

	if id ~= thisModule:GetID() then
		Log.ConsoleTrace("Gate Module: %s %d module start",name,id)
		thisModule:RegisterOtherModule(name,id)
		if name == "world" then
			thisModule:SetWorldModule(id)
		elseif name == "login" then
			thisModule:SetLoginModule(id)
		end
	end

end

return GateHandler

