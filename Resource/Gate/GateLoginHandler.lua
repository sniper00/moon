require("functions")
require("Log")
require("SerializeUtil")

local protobuf 			= require "protobuf"
local MsgID 			= require("MsgID")
local Component 		= require("Component")
local Stream 			= require("Stream")
local BinaryReader 		= require("BinaryReader")

local GateLoginHandler = class("GateLoginHandler",Component)

function GateLoginHandler:ctor()
	GateLoginHandler.super.ctor(self)

	thisService:RegisterMessageCB(MsgID.MSG_C2S_REQ_LOGIN, handler(self,self.OnRequestLogin))
	thisService:RegisterMessageCB(MsgID.MSG_S2S_SET_ACCOUNT_PLAYERID, handler(self,self.OnSetPlayerID))
	thisService:RegisterMessageCB(MsgID.MSG_S2S_SET_PLAYER_SCENEID, handler(self,self.OnSetSceneID))
	
	self.loginDatas = nil
	self.connects = nil
	self.loginService = 0

	self.sm = Stream.new()

	Log.Trace("ctor GateLoginHandler")
end

function GateLoginHandler:Start()
	GateLoginHandler.super.Start(self)

	self.loginDatas = thisService:GetComponent("LoginDatas")
	assert(nil ~= self.loginDatas,"GateLoginHandler can not get LoginDatas")

	self.connects = thisService:GetComponent("Connects")
	assert(nil ~= self.connects,"GateLoginHandler can not get Connects")
end

function GateLoginHandler:OnSessionClose( sessionID )
	Log.Trace("OnSessionClose sessionID[%u]",sessionID)
	if self.loginDatas:RemoveBySession(sessionID) then
		Log.Trace("SessionClose remove login serialNum success")
	end
end

function GateLoginHandler:OnClientClose(account, player)
	GateLoginHandler.super.OnClientClose(self,account,player)

	Log.ConsoleTrace("Client close accountID[%u] playerID[%u]",account,player)

	local sm = Stream.new()
	sm:WriteUInt16(MsgID.MSG_S2S_CLIENT_CLOSE)
	sm:WriteUInt64(account)
	sm:WriteUInt64(player)
	thisService:Broadcast(sm:Bytes())
	self.connects:RemoveByAccount(account)
end

function GateLoginHandler:OnRequestLogin(uctx,data,rpc)
	local  sessionID  =  uctx;
	if self.connects:IsLogined(sessionID) then
		local s2clogin = { ret = "LoginAgain",accountID = 0 }
		local s2cmsg = Serialize(MsgID.MSG_S2C_LOGIN_RESULT,"NetMessage.S2CLogin",s2clogin)
		thisService:SendNetMessage(sessionID,s2cmsg)
		return
	end

	local s = Deserialize("NetMessage.C2SReqLogin",data)

	Log.Trace("1.Login username[%s] password[%s]", s.username,s.password)

	local  serialNum  =  self.loginDatas:CreateSerialNum()
	local  accountID  =  Util.HashString(s.username)
	
	self.loginDatas:Add(serialNum,accountID,sessionID)

	Log.Trace("2.Login username[%s] password[%s] accountID[%u] sessionID[%d] add to login data ", s.username,s.password ,accountID,sessionID)

	-- send to login module
	self.sm:Clear()
	self.sm:WriteUInt16(MsgID.MSG_C2S_REQ_LOGIN)
	self.sm:WriteUInt64(serialNum)
	self.sm:WriteUInt64(accountID)
	self.sm:WriteString(s.password)

	if self.loginService == 0 then
		self.loginService = thisService:GetOtherService("login")
	end

	assert(thisService:GetLoginService() ~= 0, "can not find login module")

	thisService:SendRPC(thisService:GetLoginService(),self.sm:Bytes(),function (data)
		Log.Trace("3.login receive echo accountID[%u] ", accountID);
		local ld = self.loginDatas:Find(serialNum)
		if nil == ld then
			Log.Warn("3.login receive echo accountID[%u], can not find logindatas", accountID);
			return
		end
	
		local br = BinaryReader.new(data)
		local loginret = br:ReadString()
		local sn = br:ReadUInt64()
		local act = br:ReadUInt64()


		assert(serialNum == sn and  accountID == act,"login check")

		if loginret == "Ok" then
			self.connects:SetAccount(ld.sessionid, act)
			Log.Trace("4.login add Connection data sessionID:%u accountID%u]  ", ld.sessionid, act);
		end

		local s2clogin = { ret = loginret,accountID = act }
		local s2cmsg = Serialize(MsgID.MSG_S2C_LOGIN_RESULT,"NetMessage.S2CLogin",s2clogin)

		thisService:SendNetMessage(sessionID,s2cmsg)

		if false == self.loginDatas:Remove(serialNum) then
			Log.Trace("Gate remove login data [%u] success", serialNum);
		end
	end )
end

function GateLoginHandler:OnSetPlayerID(uctx,data,rpc)
	local br = BinaryReader.new(data)
	local accountID = br:ReadUInt64()
	local playerID  = br:ReadUInt64()

	local conn = self.connects:FindByAccount(accountID)
	if nil == conn then
		Log.ConsoleError("OnSetPlayerID:can not find Connection.");
	end

	self.connects:SetPlayer(accountID,playerID)
	Log.Trace("set accountid[%u] playerid:%u",accountID,playerID)
end

function GateLoginHandler:OnSetSceneID(uctx,data,rpc)
	local br = BinaryReader.new(data)
	local playerID = br:ReadUInt64()
	local sceneID = br:ReadUInt64()

	local conn = self.connects:FindByPlayer(playerID)
	if nil == conn then
		Log.ConsoleError("OnSetSceneID:can not find Connection.");
	end

	conn.sceneID = sceneID
end

return GateLoginHandler

