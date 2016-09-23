require("functions")
local Component = require("Component")
local MsgID     = require("MsgID")

local Module 	= class("Module",Component)

function Module:ctor()
    Module.super.ctor(self)
	self.OtherModules = {}
	self.RPCHandlers = {}
	self.MsgHandlers = {}
	self.RPCIncreID = 0
	self.gateModule = 0
    Log.Trace("ctor Module")
end

function Module:RegisterOtherModule(name, moduleID)
	self.OtherModules[name] = moduleID
end

function Module:GetOtherModule(name)
    return self.OtherModules[name]
end

function Module:_SendMessage(recevier,msg)
 	nativeModule:SendMessage(recevier,msg)
end

function Module:_Broadcast(msg)
 	nativeModule:BroadcastMessage(msg)
end

function Module:Send(receiver,msg,rpcid)
	rpcid = rpcid or 0
    msg:SetType(EMessageType.ModuleData)
    msg:SetReceiverRPC(rpcid)
    self:_SendMessage(receiver,msg)
    Log.Trace("Send receiver[%u] , rpcid [%u]",receiver,rpcid)
end

function Module:SendRPC(receiver,msg,cb)
	assert(nil ~= cb,"rpc callback must not be null")
	msg:SetType(EMessageType.ModuleData)
	self.RPCIncreID = 	self.RPCIncreID +1
	local id = self.RPCIncreID
	msg:SetSenderRPC(id)
	self.RPCHandlers[id] = cb
	self:_SendMessage(receiver,msg)
	Log.Trace("SendRpc  rpcid[%u]", id)
end

function Module:Broadcast(msg,msgType)
 	msg:SetType(EMessageType.ModuleData)
 	self:_Broadcast(msg)
end

function Module:SendToAccount(accountID,msg)
 	msg:SetType(EMessageType.ToClient)
 	msg:SetAccountID(accountID)
 	self:_SendMessage(self:GetGateModule(),msg)
end

function Module:SendToPlayer(playerID,msg)
 	msg:SetType(EMessageType.ToClient)
 	msg:SetPlayerID(playerID)
 	self:_SendMessage(msg)
end

function Module:RegisterMessageCB(msgID,f)
	self.MsgHandlers[msgID] = f
end

function Module:DispatchMessage(msg)
	if msg:HasReceiverRPC() then	
		local receiverRPC = msg:GetReceiverRPC()
		local f = self.RPCHandlers[receiverRPC]
		if nil ~= f then
			f(msg)
			Log.Trace("DispatchMessage Rpc[%u] success", receiverRPC)
			return true
		end
		Log.Warn("DispatchMessage Rpc[%u] failed", receiverRPC)
		return false
	end

	local br = MessageReader.new(msg)
	local msgID = br:ReadUint16()

	if msgID == MsgID.MSG_S2S_CLIENT_CLOSE then
		local accountID = br:ReadUint64()
		local playerID = br:ReadUint64()
		Module.super.OnClientClose(self,accountID,playerID)
		return true
	end

	local f = self.MsgHandlers[msgID]
	if nil == f then
		Log.ConsoleWarn("Message ID[%u] does not register!",msgID)
		return false
	end

	local UserCtx = {}

	if msg:HasSessionID() then
		UserCtx.sessionID = msg:GetSessionID()
	end

	if msg:HasAccountID() then
		UserCtx.accountID = msg:GetAccountID()
	end

	if msg:HasPlayerID() then
		UserCtx.playerID = msg:GetPlayerID()
	end

	if msg:HasSenderRPC() then
		UserCtx.senderRPC = msg:GetSenderRPC()
	end

	f(UserCtx,br)

	Log.Trace("Handler message %d", msgID)

	return true
end

function Module:PushMessage(msg)
	nativeModule:PushMessage(msg)
end

function Module:GetID()
	return nativeModule:GetID()
end

function Module:GetName()
	return nativeModule:GetName()
end

function Module:GetGateModule()
	return self.gateModule
end

function Module:SetGateModule(moduleID)
	self.gateModule = moduleID
end

return Module