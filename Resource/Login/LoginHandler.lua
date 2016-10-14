require("functions")

local SerializeUtil = require("SerializeUtil")
local MsgID 		= require("MsgID")
local Component 	= require("Component")
local SerializeUtil = require("SerializeUtil")
local Stream		= require("Stream")
local BinaryReader = require("BinaryReader")

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

function LoginHandler:OnServerStart(uctx,data,rpc)
	Log.ConsoleTrace("Login Module: server start")
	
	local s = Stream.new()
	s:WriteUInt16(MsgID.MSG_S2S_MODULE_START)
	s:WriteString(thisModule:GetName())
	s:WriteUInt32(thisModule:GetID())

	thisModule:Broadcast(s:Bytes())
end

function LoginHandler:OnModuleStart(uctx,data,rpc)
	local br   = BinaryReader.new(data)
	local name = br:ReadString()
	local id   = br:ReadUInt32()

	if id ~= thisModule:GetID() then
		Log.ConsoleTrace("Login: %s %d module start",name,id)
		thisModule:RegisterOtherModule(name,id)
		if name == "world" then
			thisModule:SetWorldModule(id)
		elseif name == "gate" then
			thisModule:SetGateModule(id)
		end
	end

end

function LoginHandler:OnRequestLogin(uctx,data,rpc)
	local br =  BinaryReader.new(data)
	local serialNum = br:ReadUInt64()
	local accountID = br:ReadUInt64()
	local password = br:ReadString()

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


	local sm  = Stream.new()
	sm:WriteString(ret)
	sm:WriteUInt64(serialNum)
	sm:WriteUInt64(accountID)

	Log.Trace("login module send to gate rpcID [%u]",rpc or 0)
	thisModule:Send(thisModule:GetGateModule(),sm:Bytes(),"",rpc)

end

return LoginHandler