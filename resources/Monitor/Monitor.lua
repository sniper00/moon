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

function Monitor:OnMessage( ... )
    -- ingnore user message
end

function Monitor:OnSystem(msg,msgtype)
    if msgtype == message_type.system_service_crash then
        local data = msg:bytes()
        local br = BinaryReader.new(data)
        --Log.ConsoleTrace("Service Crashed:%s",br:ReadString())
    elseif msgtype == message_type.system_network_report then
        local data = msg:bytes()
        local br = BinaryReader.new(data)
        --Log.ConsoleTrace("network report:%s",br:ReadString())
    end
end

return Monitor
