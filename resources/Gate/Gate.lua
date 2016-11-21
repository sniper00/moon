package.path    = 'Base/?.lua;Gate/?.lua;'
require("functions")
require("Log")
require("ConfigLoader")
require("SerializeUtil")

local Service       = require("Service")
local Connects      = require("Connect")
local MsgID         = require("MsgID")
local BinaryReader  = require("BinaryReader")
local GateHandler   = require("GateHandler")
local GateLoginHandler = require("GateLoginHandler")
local LoginDatas    = require("LoginDatas")


local Gate = class("Gate", Service)

local messageCount = 0

function Gate:ctor()
    Log.SetTag("Gate")

    Gate.super.ctor(self)

    self.connects = nil
    self.net = nil
    self.gateLoginHandler = nil

    self.worldService = 0
    self.loginService = 0
    self.ID          = 0

    LoadProtocol();

    Log.Trace("ctor Gate")
end

function Gate:Init(config)
    self.ID = Gate.super.GetID(self)
    Gate.super.SetGateService(self,self.ID)

    local ip =  Gate.super.GetConfig(self,"ip")
    local port = Gate.super.GetConfig(self,"port")

    Gate.super.Listen(self,ip,port)


    Gate.super.AddComponent(self,"Connects", Connects.new())
    Gate.super.AddComponent(self,"GateHandler", GateHandler.new())
    Gate.super.AddComponent(self,"GateLoginHandler",GateLoginHandler.new())
    Gate.super.AddComponent(self,"LoginDatas",LoginDatas.new())

    self.connects = Gate.super.GetComponent(self,"Connects")
    self.gateLoginHandler = Gate.super.GetComponent(self,"GateLoginHandler")

    Log.Trace("Gate Service Init: %s",config)

    -- Timer:Repeat(1000,-1,function ()
    --     Log.ConsoleTrace("Msg Counter: %d",messageCount)
    --     messageCount = 0
    -- end)
end

local onf =  Gate.super.OnMessage

function Gate:OnMessage(msg,msgtype)
    if msgtype == MessageType.NetworkConnect then
         self:ClientConnect(msg)
    elseif msgtype == MessageType.NetworkData then
        self:ClientData(msg)
    elseif msgtype == MessageType.NetworkClose then
        self:ClientClose(msg)
    elseif msgtype == MessageType.ToClient then
        self:ToClientData(msg)
    else
        onf(self,msg,msgtype)
    end
end

function Gate:ClientConnect(msg)
    local data = msg:bytes()

    local br = BinaryReader.new(data)
    Log.ConsoleTrace("CLIENT CONNECT: %s",br:ReadString())
end

local mfunc = Gate.super.OnMessageServiceData

function Gate:ClientData(msg)
    messageCount = messageCount+1

    local sessionid = msg:sender()
    local data = msg:bytes()
    local msgid,msgdata = UnpackMsg(data)

    local handled = mfunc(self,sessionid,msgid,msgdata,nil,0,MessageType.NetworkData)
    if handled then      
        return
    end

    local conn = self.connects:Find(sessionid)
    if nil == conn then
        Log.ConsoleWarn("Illegal Msg: client not authenticated.")
        return
    end

    if msgid > MsgID.MSG_MUST_HAVE_PLAYERID then
        Log.ConsoleWarn("Illegal Msg: client not login msgID[%u].",msgid)
        return
    end

    --------------------------------------------------------

    if conn.sceneID ~= 0 then

    else
        if self.worldService == nil then
            self.worldService = Gate.super.GetOtherService("World")
            assert(nil ~= self.worldService)
        end
    end
end

function Gate:ClientClose(msg)
    local data = msg:bytes()
    local sessionid = msg:sender()
    local br = BinaryReader.new(data)
    Log.ConsoleTrace("CLIENT CLOSE: %s %s  %s",br:ReadString(),br:ReadString(),br:ReadString())

    self.gateLoginHandler:OnSessionClose(sessionid)

    local conn = self.connects:Find(sessionid)
    if nil == conn then
        return
    end

    Gate.super.OnClientClose(self,conn.accountid, conn.playerid)

    if self.connects:Remove(sessionid) then

    else
        assert(false)
    end
end

function Gate:ToClientData(msg)
    local userdata = msg:GetUserData()
    local uctx = DeserializeUserData(userdata)
    local sessionID = 0
    if uctx.playerid ~= 0 then
    	local conn = self.connects:FindByPlayer(uctx.playerid)
        sessionID = conn.sessionid
    else
        local conn = self.connects:FindByAccount(uctx.accountid)
        assert(nil ~= conn)
        sessionID = conn.sessionid
    end

    --if nil == sessionID or 0 == sessionID then
    --    return
    --end

    print("send to client--",sessionID)
    self.net:Send(sessionID,msg)
end

function  Gate:SendNetMessage(sessionID,data)
    local msgid = UnpackMsg(data)
    --Log.Trace("send to client %u, data len %d,msgid%d",sessionID,string.len(data),msgid)
    nativeService:net_send(sessionID,data,string.len(data))
end

function Gate:SetWorldService(moduleid)
    self.worldService = moduleid
end

function Gate:GetWorldService()
    return self.worldService
end

function Gate:SetLoginService(moduleid)
    self.loginService = moduleid
end

function Gate:GetLoginService()
    return self.loginService
end

function Gate:OnError(msg)
    
end

return Gate
