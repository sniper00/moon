package.path    = 'Base/?.lua;Monitor/?.lua;'

require("functions")
require("Log")

local Service         = require("Service")
local MonitorHandler = require("MonitorHandler")
local BinaryReader   = require("BinaryReader")

local Monitor = class("Monitor", Service)

function Monitor:ctor()
    Log.SetTag("Monitor")
    Monitor.super.ctor(self)
    Monitor.super.SetEnableUpdate(self,false)
    Log.Trace("ctor Monitor")
end

function Monitor:Init(config)
    Monitor.super.AddComponent(self,"MonitorHandler", MonitorHandler.new())
    Log.Trace("Monitor Service Init: %s",config)
end

function Monitor:OnMessage(sender,data,userdata,rpcid,msgtype)
    if msgtype == MessageType.ServiceCrash then
        local br = BinaryReader.new(data)
        local moduleID = br:ReadUInt32()
        local moduleName = br:ReadString()
        Log.ConsoleTrace("Service ID:%d  Name:%s  Crashed",moduleID,moduleName)
    else
        Monitor.super.OnMessage(self,sender,data,userdata,rpcid,msgtype)
    end
end

return Monitor
