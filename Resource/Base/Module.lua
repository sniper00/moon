require("functions")
require("SerializeUtil")

local Component 	= require("Component")
local MsgID     	= require("MsgID")
local BinaryReader 	= require("BinaryReader")

local Module 		= class("Module",Component)

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

function Module:_Send(recevier,data,userdata,rpc,msgtype)
 	nativeModule:Send(recevier,data,userdata,rpc,msgtype)
end

function Module:_Broadcast(data,userdata,msgtype)
 	nativeModule:Broadcast(data,userdata,msgtype)
end

function Module:Send(receiver,data,userdata,rpc,msgtype)

	userdata 	= userdata or ""
	rpc 		= rpc or 0

    if 0 ~= rpc then
    	msgtype = msgtype or EMessageType.ModuleRPC
    else
    	msgtype = msgtype or EMessageType.ModuleData
	end

    self:_Send(receiver,data,userdata,rpc,msgtype)

    Log.Trace("Send receiver[%u] ,msgType[%d] rpcid [%u]",receiver,msgtype,rpc)
end

function Module:SendRPC(receiver,data,userdata,cb)
	assert(nil ~= cb,type(cb) == "function","rpc callback must not be null")

	self.RPCIncreID 		= self.RPCIncreID +1
	local 		id 			= self.RPCIncreID
	self.RPCHandlers[id] 	= cb

	self:_Send(receiver,data,userdata,id,EMessageType.ModuleData)
	
	Log.Trace("SendRpc  rpcid[%u]", id)
end

function Module:Broadcast(data,userdata,msgtype)
	msgtype = msgtype or EMessageType.ModuleData
	userdata = userdata or ""
 	self:_Broadcast(data,userdata,msgtype)
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

function Module:DispatchMessage(sender,data,userdata,rpc,msgtype)
	if msgtype == EMessageType.ModuleRPC  then
		if rpc ~=0 then	
			local f = self.RPCHandlers[rpc]
			if nil ~= f then
				f(data,userData)
				self.RPCHandlers[rpc] = nil
				Log.Trace("DispatchMessage Rpc[%u] success", rpc)
				return true
			end
			Log.Warn("DispatchMessage Rpc[%u] failed", rpc)
			return false
		end
		Log.Warn("DispatchMessage Rpc failed, has no rcp id")
		return false
	end

	local msgID,msgData = UnpackMsg(data)

	if msgID == MsgID.MSG_S2S_CLIENT_CLOSE then
		local br = BinaryReader.new(msgData)
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

	local uctx = sender
	if nil ~= userdata then
		uctx = DeserializeUserData(userdata)
	end

	f(uctx,msgData,rpc)

	--Log.Trace("Handler message %d", msgID)
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