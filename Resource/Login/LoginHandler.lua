require("functions")

local SerializeUtil = require("SerializeUtil")
local MsgID 		= require("MsgID")
local Component 	= require("Component")
local SerializeUtil = require("SerializeUtil")

local LoginHandler = class("LoginHandler",Component)

function LoginHandler:ctor()
	LoginHandler.super.ctor(self)
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SERVER_START, handler(self,self.OnServerStart))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_MODULE_START, handler(self,self.OnModuleStart))
	thisModule:RegisterMessageCB(MsgID.MSG_C2S_REQ_LOGIN, handler(self,self.OnRequestLogin))
	
	Log.Trace("ctor LoginHandler")
end

function LoginHandler:OnClientClose( accountID,playerID )
	if self.accountDatas:OffLine(accountID) then
		Log.Trace("LoginHandler Client OffLine success")
	else
		Log.Warn("LoginHandler Client OffLine failed")
	end
end

function LoginHandler:Start()
	LoginHandler.super.Start(self)
	self.accountDatas = thisModule:GetComponent("AccountDatas")
	assert(nil ~= self.accountDatas)
end

function LoginHandler:OnServerStart(userCtx, data)
	local msg,mw = SerializeUtil.SerializeEx(MsgID.MSG_S2S_MODULE_START)
	mw:WriteString(thisModule:GetName())
	mw:WriteUint32(thisModule:GetID())
	thisModule:Broadcast(msg)
	Log.ConsoleTrace("Login Module: server start")
end

function LoginHandler:OnModuleStart(userCtx, data)
	local name = data:ReadString()
	local id   = data:ReadUint32()
	if id ~= thisModule:GetID() then
		Log.ConsoleTrace("Login : %s %d module start",name,id)
		thisModule:RegisterOtherModule(name,id)
		if name == "world" then
			thisModule:SetWorldModule(id)
		elseif name == "gate" then
			thisModule:SetGateModule(id)
		end
	end

end

function LoginHandler:OnRequestLogin(userCtx, data)
	local serialNum = data:ReadUint64()
	local accountID = data:ReadUint64()
	local password = data:ReadString()

	local ret = "Ok"


	if self.accountDatas:IsOnline(accountID) then
		ret = "LoginAgain"
	else
		self.accountDatas:AddAccount(accountID,"",password)
		if self.accountDatas:OnLine(accountID) then
			Log.Trace("login module login success [%u]", accountID)
		else
			Log.Warn("login module login online failed [%u]", accountID)
		end
	end

	local msg = CreateMessage()
	local mw  = MessageWriter.new(msg)
	mw:WriteString(ret)
	mw:WriteUint64(serialNum)
	mw:WriteUint64(accountID)

	Log.Trace("login module send to gate rpcID [%u]",userCtx.senderRPC)
	thisModule:Send(thisModule:GetGateModule(),msg,userCtx.senderRPC)

end

return LoginHandler