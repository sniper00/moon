require("functions")
local Component = require("Component")

local Network = class("Network",Component)

function Network:ctor()
   Network.super.ctor(self)
   Network.super.SetEnableUpdate(self,true)

   self.net = CreateNetwork()

   Log.Trace("ctor Network")
end

function Network:Init(v)
    self.net:InitNet(v)
    --self.net.OnHandleNetMessage = function(msg) self.OnMessage(self,msg) end
end

function Network:SetCB(f)
    assert(type(f)=="function")
    self.net.OnHandleNetMessage = f;
end

function Network:Listen(ip,port)
    self.net:Listen(ip, port)
end

function Network:Connect(ip,port)
    return self.net:SyncConnect(ip,port)
end

function Network:Start()
   Log.Trace("Network Start")
   self.net:OnEnter()
end

function Network:Update(a)
    self.net:Update(a)
end

function Network:Destory()
    self.net:OnExit()
    Log.Trace("Network Stop")
end

function Network:Send(sessionID, msg)
    self.net:SendNetMessage(sessionID,msg)
end

function Network:Native()
    return self.net
end

return Network