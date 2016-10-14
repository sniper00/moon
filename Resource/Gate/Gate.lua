package.path    = 'Base/?.lua;Gate/?.lua;'

require("functions")
require("Log")
require("ConfigLoader")
require("SerializeUtil")

local Module        = require("Module")
local Network       = require("Network")
local Connects      = require("Connect")
local MsgID         = require("MsgID")
local BinaryReader  = require("BinaryReader")
local GateHandler   = require("GateHandler")
local GateLoginHandler = require("GateLoginHandler")
local LoginDatas    = require("LoginDatas")


local Gate = class("Gate", Module)


function Gate:ctor()
    Gate.super.ctor(self)

    self.connects = nil
    self.net = nil
    self.gateLoginHandler = nil

    self.worldModule = 0
    self.loginModule = 0
    self.ID          = 0

    LoadProtocol();

    Log.Trace("ctor Gate")
end

function Gate:Init(config)
    self.ID = Gate.super.GetID(self)
    Gate.super.SetGateModule(self,self.ID)

    local kvconfig = string.parsekv(config)

    kvconfig.netthread = kvconfig.netthread or "1"
    assert(kvconfig.ip,"Gate ip is nil!")
    assert(kvconfig.port,"Gate port is nil!")

    self.net = Network.new()
    self.net:Init(tonumber(kvconfig.netthread))
    self.net:Listen(kvconfig.ip,kvconfig.port)
    self.net:SetHandler(handler(self,self.OnNetMessage))
    
    Gate.super.AddComponent(self,"Network", self.net)
    Gate.super.AddComponent(self,"Connects", Connects.new())
    Gate.super.AddComponent(self,"GateHandler", GateHandler.new())
    Gate.super.AddComponent(self,"GateLoginHandler",GateLoginHandler.new())
    Gate.super.AddComponent(self,"LoginDatas",LoginDatas.new())

    self.connects = Gate.super.GetComponent(self,"Connects")
    self.gateLoginHandler = Gate.super.GetComponent(self,"GateLoginHandler")

    Log.Trace("Gate Module Init: %s",config)

end

function Gate:OnNetMessage(sessionid,data,msgtype)
    self:OnMessage(sessionid,data,"",0,msgtype)
end

function Gate:OnMessage(sender,data,userdata,rpcid,msgtype)
    if msgtype == EMessageType.NetworkConnect then
        self:ClientConnect(data)
    elseif msgtype == EMessageType.NetworkData then
        self:ClientData(sender,data,msgtype)
    elseif msgtype == EMessageType.NetworkClose then
        self:ClientClose(sender,data)
    elseif msgtype == EMessageType.ModuleData or msgtype == EMessageType.ModuleRPC then
        self:ModuleData(sender,data,userdata,rpcid,msgtype)
    elseif msgtype == EMessageType.ToClient then
        self:ToClientData(data,userdata)
    end
end

function Gate:ClientConnect(data)
    local br = BinaryReader.new(data)
    Log.ConsoleTrace("CLIENT CONNECT: %s",br:ReadString())
end

function Gate:ClientData(sessionid,data,msgtype)
    
    if Gate.super.DispatchMessage(self,sessionid,data,nil,0,msgtype) then
        return
    end

    local conn = self.connects:Find(sessionid)
    if nil == conn then
        Log.ConsoleWarn("Illegal Msg: client not connected.")
        return
    end

    local msgID,n = string.unpack("=H",data)

    if msgID > MsgID.MSG_MUST_HAVE_PLAYERID then
        Log.ConsoleWarn("Illegal Msg: client not login msgID[%u].",msgID)
        return
    end

    --------------------------------------------------------

    if conn.sceneID ~= 0 then

    else
        if self.worldModule == nil then
            self.worldModule = Gate.super.GetOtherModule("World")
            assert(nil ~= self.worldModule)
        end
    end
end

function Gate:ClientClose(sessionid,data)
    local br = BinaryReader.new(data)
    Log.ConsoleTrace("CLIENT CLOSE: %s",br:ReadString())

    self.gateLoginHandler:OnSessionClose(sessionid)

    local conn = self.connects:Find(sessionid)
    if nil == conn then
        return
    end

    Gate.super.OnClientClose(self,conn.accountID, conn.playerID)

    if self.connects:Remove(sessionID) then

    else
        assert(0)
    end
end

function Gate:ModuleData(sender,data,userdata,rpcid,msgtype)
    Gate.super.DispatchMessage(self,sender,data,userdata,rpcid,msgtype)
end

function Gate:ToClientData(data,userdata)
    local sessionID = 0
    if msg:IsPlayerID() then
    	local conn = self.connexts.FindByPlayer(msg:GetPlayerID())
        sessionID = conn.sessionID
    else
        local conn = self.connexts.FindByAccount(msg:GetAccountID())
        assert(nil ~= conn)
        sessionID = conn.sessionID
    end

    if 0 == sessionID then
        return
    end
    print("send to client--",sessionID)
    self.net:Send(sessionID,data)
end

function  Gate:SendNetMessage(sessionID,data)
    Log.Trace("send to client %u, data len %d",sessionID,string.len(data))
    self.net:Send(sessionID,data)
end

function Gate:SetWorldModule(moduleid)
    self.worldModule = moduleid
end

function Gate:GetWorldModule()
    return self.worldModule
end


function Gate:SetLoginModule(moduleid)
    self.loginModule = moduleid
end

function Gate:GetLoginModule()
    return self.loginModule
end

return Gate
