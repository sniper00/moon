package.path 	= 'Base/?.lua;Login/?.lua;'

require("functions")

local protobuf          = require("protobuf")
local Module            = require("Module")
local AccountDatas  	= require("AccountDatas")
local LoginHandler 		= require("LoginHandler")

local Login 			= class("Login",Module)

function Login:ctor()
    Login.super.ctor(self)

    self.worldModule = nil
    
    Log.Trace("ctor Login")
end

function Login:Init(config)
	Login.super.AddComponent(self, "AccountDatas",AccountDatas.new())
	Login.super.AddComponent(self,"LoginHandler",LoginHandler.new())
	Log.Trace("Module Init: %s",config)
end

function Login:OnMessage(msg)
	Login.super.DispatchMessage(self, msg)
end


function Login:SetWorldModule(moduleid)
    self.worldModule = moduleid
end

function Login:GetWorldModule()
    return self.worldModule
end

return Login
