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
end

function Network:SetHandler(f)
    assert(type(f)=="function")
    self.net:SetHandler(f);
end

function Network:Listen(ip,port)
    self.net:Listen(ip, port)
end

function Network:Connect(ip,port)
    return self.net:SyncConnect(ip,port)
end

function Network:Start()
   Log.Trace("Network Start")
   self.net:Start()
end

function Network:Update(a)
    self.net:Update(a)
end

function Network:Destory()
    self.net:Destory()
    Log.Trace("Network Stop")
end

function Network:Send(sessionID, data)
    self.net:Send(sessionID,data)
end

-- 超时时间 单位 s
function Network:SetTimeout(timeout)
  self.net:SetTimeout(timeout)
end

function Network:Native()
    return self.net
end

return Network