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
    
    LoadProtocol();

    Log.Trace("ctor Login")
end

function Login:Init(config)
	Login.super.AddComponent(self, "AccountDatas",AccountDatas.new())
	Login.super.AddComponent(self,"LoginHandler",LoginHandler.new())
	Log.Trace("Service Init: %s",config)

    -- Timer:Repeat(5000,-1,function ()
    --     Log.ConsoleTrace("Login Memeory Counter: "..collectgarbage("count"))
    -- end)
end

function Login:SetWorldService(moduleid)
    self.worldService = moduleid
end

function Login:GetWorldService()
    return self.worldService
end

return Login
