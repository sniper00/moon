package.path 	= 'Base/?.lua;Login/?.lua;'

require("functions")
require("ConfigLoader")

local protobuf          = require("protobuf")
local Service            = require("Service")
local AccountDatas  	= require("AccountDatas")
local LoginHandler 		= require("LoginHandler")

local Login 			= class("Login",Service)

function Login:ctor()
    Log.SetTag("Login")

    Login.super.ctor(self)

    Login.super.SetEnableUpdate(self,true)

    self.worldService = nil
    self.mysql = nil
    
    LoadProtocol();

    Log.Trace("ctor Login")
end

function Login:Init(config)
	Login.super.AddComponent(self, "AccountDatas",AccountDatas.new())
	Login.super.AddComponent(self,"LoginHandler",LoginHandler.new())
	Log.Trace("Service Init: %s",config)
end

function Login:SetWorldService(moduleid)
    self.worldService = moduleid
end

function Login:GetWorldService()
    return self.worldService
end

function Login:SetMysqlService(moduleid)
    self.mysql = moduleid
end

function Login:GetMysqlService()
    return self.mysql
end

return Login
