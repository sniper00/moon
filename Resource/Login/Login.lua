package.path 	= 'Base/?.lua;Login/?.lua;'
package.cpath 	= 'Base/?.so;Base/?.dll;./?.so;./?.dll;'
require("functions")
local protobuf          = require ("protobuf")
local Module            = require("Module")
local AccountDatas  	= require("AccountDatas")
local LoginHandler 		= require("LoginHandler")

local Login = class("Login",Module)

function Login:ctor()
    Login.super.ctor(self)

    self.worldModule = nil
    
    Log.Trace("ctor Login")
end

function Login:Init(config)
	local accountDatas = AccountDatas.new()
	Login.super.AddComponent(self, "AccountDatas", accountDatas)

	local loginHandler = LoginHandler.new()
	Login.super.AddComponent(self,"LoginHandler",loginHandler)

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
