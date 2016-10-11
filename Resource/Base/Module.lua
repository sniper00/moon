require("functions")
local Component = require("Component")
local MsgID     = require("MsgID")
local BinaryReader = require("BinaryReader")

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
 	nativeModule:Send(recevier,msg)
end

function Module:_Broadcast(msg)
 	nativeModule:Broadcast(msg)
end

function Module:Send(receiver,msg,rpcid,msgType)
	rpcid = rpcid or 0
	
    if 0 ~= rpcid then
    	msg:SetRPCID(rpcid)
    	msgType = msgType or EMessageType.ModuleRPC
    else
    	msgType = msgType or EMessageType.ModuleData
	end

	msg:SetType(msgType)

    self:_SendMessage(receiver,msg)
    Log.Trace("Send receiver[%u] ,msgType[%d] rpcid [%u]",receiver,msgType,rpcid)
end

function Module:SendRPC(receiver,msg,cb)
	assert(nil ~= cb,"rpc callback must not be null")
	msg:SetType(EMessageType.ModuleData)
	self.RPCIncreID = 	self.RPCIncreID +1
	local id = self.RPCIncreID
	msg:SetRPCID(id)
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
	if msg:GetType() == EMessageType.ModuleRPC  then
		if msg:HasRPCID() then	
			local rpcID = msg:GetRPCID()
			local f = self.RPCHandlers[rpcID]
			if nil ~= f then
				f(msg)
				self.RPCHandlers[rpcID] = nil
				Log.Trace("DispatchMessage Rpc[%u] success", rpcID)
				return true
			end
			Log.Warn("DispatchMessage Rpc[%u] failed", rpcID)
			return false
		end
		Log.Warn("DispatchMessage Rpc failed, has no rcp id")
		return false
	end

	local br   	= BinaryReader.new(msg:Bytes())
	local msgID = br:ReadUInt16()

	if msgID == MsgID.MSG_S2S_CLIENT_CLOSE then
		local accountID = br:ReadUInt64()
		local playerID 	= br:ReadUInt64()
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

	UserCtx.playerID = msg:GetSubUserID()
	if UserCtx.playerID == 0 then
		UserCtx.accountID = msg:GetUserID()
	end

	if msg:HasRPCID() then
		UserCtx.rpcID = msg:GetRPCID()
	end

	f(UserCtx,br)

	Log.Trace("Handler message %d", msgID)
	return true
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