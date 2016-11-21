require("functions")
require("SerializeUtil")

local Component = require("Component")
local MsgID = require("MsgID")
local BinaryReader = require("BinaryReader")

local Service = class("Service",Component)

function Service:ctor()
    Service.super.ctor(self)
	self.OtherServices = {}
	self.RPCHandlers = {}
	self.MsgHandlers = {}
	self.RPCIncreID = 0
	self.gateService = 0

	self.MsgHandlers[MsgID.MSG_S2S_CLIENT_CLOSE] = function()end

    Log.Trace("ctor Service")
end

function Service:RegisterOtherService(name, serviceid)
	self.OtherServices[name] = serviceid
end

function Service:GetOtherService(name)
    return self.OtherServices[name]
end

function Service:_Send(recevier,data,userctx,rpc,msgtype)
 	nativeService:send(recevier,data,userctx,rpc,msgtype)
end

function Service:_Broadcast(data)
 	nativeService:broadcast(data)
end

function Service:Send(receiver,data,userctx,rpc,msgtype)
	userctx = userctx or ""
	rpc 		= rpc or 0

    if 0 ~= rpc then
    	msgtype = msgtype or MessageType.ServiceRPC
    else
    	msgtype = msgtype or MessageType.ServiceData
	end

    self:_Send(receiver,data,userctx,rpc,msgtype)

    --Log.Trace("Send receiver[%u] ,msgType[%d] rpcid [%u]",receiver,msgtype,rpc)
end

function Service:SendRPC(receiver,data,cb)
	assert(nil ~= cb,type(cb) == "function","rpc callback must not be null")

	self.RPCIncreID 		= self.RPCIncreID +1
	if self.RPCIncreID == 0xFFFFFFF then
		self.RPCIncreID = 1
	end

	local 		id 			= self.RPCIncreID
	self.RPCHandlers[id] 	= cb
	self:_Send(receiver,data,"",id,MessageType.ServiceData)
	--Log.Trace("SendRpc  rpcid[%u] receiver[%u]",id,receiver)
end

function Service:Broadcast(data)
 	self:_Broadcast(data)
end

function Service:SendToAccount(accountID,data)
 	local msgtype = MessageType.ToClient
 	local userctx = SerializeUserData(0,accountID,0)
 	self:_Send(self:GetGateService(),data,userctx,0,msgtype)
end

function Service:SendToPlayer(playerID,data)
 	local msgtype = MessageType.ToClient
 	local userctx = SerializeUserData(0,0,playerID)
 	self:_Send(self:GetGateService(),data,userctx,0,msgtype)
end

function Service:RegisterMessageCB(msgID,f)
	self.MsgHandlers[msgID] = f
end

function Service:IsRegister(msgID)
	return self.MsgHandlers[msgID] ~= nil
end

function Service:OnMessage(msg,msgtype)
	local data = msg:bytes()
	local rpc = msg:rpc()
	local msgtype = msg:type()
	if msgtype == MessageType.ServiceRPC then
		self:OnMessageRpc(data,rpc)
	elseif msgtype == MessageType.ServiceData or  msgtype == MessageType.NetworkData or msgtype == MessageType.ServiceBroadcast then
		local sender = msg:sender()
		local userctx = msg:userctx()
		local msgid,msgdata = UnpackMsg(data)
		self:OnMessageServiceData(sender,msgid,msgdata,userctx,rpc,msgtype)
    end
end

function Service:OnMessageRpc(data,rpc)
	if rpc ~=0 then	
		local f = self.RPCHandlers[rpc]
		if nil ~= f then
			f(data,userData)
			self.RPCHandlers[rpc] = nil
			--Log.Trace("DispatchMessage Rpc[%u] success", rpc)
			return
		end
		Log.Warn("DispatchMessage Rpc[%u] failed", rpc)
	end
	Log.Warn("DispatchMessage Rpc failed, has no rcp id")
end

function Service:OnMessageServiceData(sender,msgid,msgdata,userdata,rpc,msgtype)
	--Log.Trace("OnMessageServiceData sender%u  MsgID %d begin",sender, msgid)
	if msgid == MsgID.MSG_S2S_CLIENT_CLOSE then
		local br = BinaryReader.new(msgdata)
		local accountID = br:ReadUInt64()
		local playerID 	= br:ReadUInt64()
		Service.super.OnClientClose(self,accountID,playerID)
		return true
	end

	local f = self.MsgHandlers[msgid]
	if nil == f then
		return false
	end
	assert(sender ~= nil)

	local uctx = sender
	if nil ~= userdata then
		uctx = DeserializeUserData(userdata)
	end
	
	f(uctx,msgdata,rpc)

	--Log.Trace("OnMessageServiceData MsgID %d end", msgid)
	return true
end


function Service:GetID()
	return nativeService:serviceid()
end

function Service:GetName()
	return nativeService:name()
end

function Service:NewService( config )
	nativeService:new_service(config)
end

function Service:RemoveService( id )
	nativeService:remove_service(id)
end

function Service:GetGateService()
	return self.gateService
end

function Service:SetGateService(moduleID)
	self.gateService = moduleID
end

function Service:SetEnableUpdate(v)
	nativeService:set_enbale_update(v)
end

function Service:GetConfig(key)
	return nativeService:get_config(key)
end

function Service:Listen(ip,port)
	nativeService:listen(ip,port)
end

function Service:SendNet(sessionid,data)
	nativeService:net_send(sessionid,data)
end

return Service