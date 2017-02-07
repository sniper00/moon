
require("functions")

local Serializer = require("Serializer")
local Component = require("Component")
local MsgID = require("MsgID")
local stream_reader = require("stream_reader")

local ServiceHelper = class("ServiceHelper", Component)

function ServiceHelper:ctor()
    ServiceHelper.super.ctor(self)
    self.OtherServices = { }
    self.RPCHandlers = { }
    self.MsgHandlers = { }
    self.RPCIncreID = 0
    self.gateService = 0
    self.cservice = _service

    self.MsgHandlers[MsgID.MSG_S2S_CLIENT_CLOSE] = function() end

    Log.Trace("ctor ServiceHelper")
end

function ServiceHelper:RegisterOtherService(name, serviceid)
    self.OtherServices[name] = serviceid
end

function ServiceHelper:GetOtherService(name)
    return self.OtherServices[name]
end

function ServiceHelper:_Send(recevier, data, userctx, rpc, msgtype)
    self.cservice:send(recevier, data, userctx, rpc, msgtype)
end

function ServiceHelper:Broadcast(data, msgtype)
    msgtype = msgtype or message_type.service_send
    self.cservice:broadcast(data, msgtype)
end

function ServiceHelper:MakeCache(data)
    return self.cservice:make_cache(data)
end

function ServiceHelper:SendCache(receiver, cacheid, userctx, rpc, msgtype)
    userctx = userctx or ""
    rpc = rpc or 0

    if 0 ~= rpc then
        msgtype = msgtype or message_type.service_response
    else
        msgtype = msgtype or message_type.service_send
    end

    self.cservice:send_cache(receiver, cacheid, userctx, rpc, msgtype)
end

function ServiceHelper:Send(receiver, data, userctx, rpc, msgtype)
    userctx = userctx or ""
    rpc = rpc or 0

    if 0 ~= rpc then
        msgtype = msgtype or message_type.service_response
    else
        msgtype = msgtype or message_type.service_send
    end

    self:_Send(receiver, data, userctx, rpc, msgtype)

    -- Log.Trace("Send receiver[%u] ,msgType[%d] rpcid [%u]",receiver,msgtype,rpc)
end

function ServiceHelper:SendRPC(receiver, data, cb)
    assert(nil ~= cb, type(cb) == "function", "rpc callback must not be null")

    self.RPCIncreID = self.RPCIncreID + 1
    if self.RPCIncreID == 0xFFFFFFF then
        self.RPCIncreID = 1
    end

    local id = self.RPCIncreID
    self.RPCHandlers[id] = cb
    self:_Send(receiver, data, "", id, message_type.service_send)
    -- Log.Trace("SendRpc  rpcid[%u] receiver[%u]",id,receiver)
end

function ServiceHelper:SendToPlayer(playerID, data)
    local msgtype = message_type.service_client
    local userctx = Serializer.PackUserctx(0, playerID)
    self:_Send(self:GetGateService(), data, userctx, 0, msgtype)
end

function ServiceHelper:SendToPlayerByCache(playerID, cacheid)
    local msgtype = message_type.service_client
    local userctx = Serializer.PackUserctx(0, playerID)
    self.cservice:send_cache(self:GetGateService(), cacheid, userctx, 0, msgtype)
end

function ServiceHelper:RegisterMessageCB(msgID, f)
    self.MsgHandlers[msgID] = f
end

function ServiceHelper:IsRegister(msgID)
    return self.MsgHandlers[msgID] ~= nil
end

function ServiceHelper:OnMessage(msg, msgtype)
    local data = msg:bytes()
    local rpc = msg:rpc()
    -- Log.ConsoleTrace("OnMessage type %d", msgtype)
    if msgtype == message_type.service_response then
        self:OnMessageRpc(data, rpc)
    else
        local sender = msg:sender()
        local userctx = msg:userctx()
        local msgid, msgdata = Serializer.UnpackMsg(data)
        self:OnMessageServiceData(sender, msgid, msgdata, userctx, rpc, msgtype)
    end
end

function ServiceHelper:OnMessageRpc(data, rpc)
    if rpc ~= 0 then
        local f = self.RPCHandlers[rpc]
        if nil ~= f then
            f(data)
            self.RPCHandlers[rpc] = nil
            -- Log.Trace("DispatchMessage Rpc[%u] success", rpc)
            return
        end
        Log.Warn("DispatchMessage Rpc[%u] failed", rpc)
    end
    Log.Warn("DispatchMessage Rpc failed, has no rcp id")
end

function ServiceHelper:OnMessageServiceData(sender, msgid, msgdata, userdata, rpc, msgtype)
    --Log.ConsoleTrace("OnMessageServiceData	sender%d MsgID %d",sender, msgid)
    if msgid == MsgID.MSG_S2S_CLIENT_CLOSE then
        local sr = stream_reader.new(msgdata)
        local playerID = sr:read_uint32()
        --Log.ConsoleTrace("Client Close	playerID%u",playerID)
        ServiceHelper.super.OnClientClose(self, playerID)
        return true
    end

    local f = self.MsgHandlers[msgid]
    if nil == f then
        return false
    end
    assert(sender ~= nil)

    local uctx = sender
    if nil ~= userdata then
        uctx = Serializer.UnpackUserctx(userdata)
    end

    f(uctx, msgdata, rpc)

    -- Log.Trace("OnMessageServiceData MsgID %d end", msgid)
    return true
end


function ServiceHelper:GetID()
    return self.cservice:serviceid()
end

function ServiceHelper:GetName()
    return self.cservice:name()
end

function ServiceHelper:NewService(service_type, config)
    self.cservice:new_service(service_type, config)
end

function ServiceHelper:RemoveService(id)
    self.cservice:remove_service(id)
end

function ServiceHelper:GetGateService()
    return self.gateService
end

function ServiceHelper:SetGateService(moduleID)
    self.gateService = moduleID
end

function ServiceHelper:SetEnableUpdate(v)
    self.cservice:set_enable_update(v)
end

function ServiceHelper:GetConfig(key)
    return self.cservice:get_config(key)
end

function ServiceHelper:Listen(ip, port)
    self.cservice:listen(ip, port)
end

function ServiceHelper:SendNet(sessionid, data)
    self.cservice:net_send(sessionid, data)
end

return ServiceHelper