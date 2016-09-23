require("functions")
require("Log")

local protobuf 			= require "protobuf"
local MsgID 			= require("MsgID")
local Component 		= require("Component")
local SerializeUtil 	= require("SerializeUtil")

local GateLoginHandler = class("GateLoginHandler",Component)

function GateLoginHandler:ctor()
	GateLoginHandler.super.ctor(self)

	thisModule:RegisterMessageCB(MsgID.MSG_C2S_REQ_LOGIN, handler(self,self.OnRequestLogin))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SET_ACCOUNT_PLAYERID, handler(self,self.OnSetPlayerID))
	thisModule:RegisterMessageCB(MsgID.MSG_S2S_SET_PLAYER_SCENEID, handler(self,self.OnSetSceneID))
	
	self.loginDatas = nil
	self.connects = nil
	self.loginModule = 0

	Log.Trace("ctor GateLoginHandler")
end

function GateLoginHandler:Start()
	GateLoginHandler.super.Start(self)

	self.loginDatas = thisModule:GetComponent("LoginDatas")
	assert(nil ~= self.loginDatas,"GateLoginHandler can not get LoginDatas")

	self.connects = thisModule:GetComponent("Connects")
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

	local msg,mw = SerializeUtil.SerializeEx(MsgID.MSG_S2S_CLIENT_CLOSE)
	mw:WriteUint64(account)
	mw:WriteUint64(player)

	thisModule:Broadcast(msg)

	self.connects:RemoveByAccount(account)
end

function GateLoginHandler:OnRequestLogin(userctx,data)
	Log.Trace("Message Size: %d",data:Size())
	local s = SerializeUtil.Deserialize("NetMessage.C2SReqLogin",data)

	Log.Trace("1.Login username[%s] password[%s]", s.username,s.password)

	local  serialNum  =  self.loginDatas:CreateSerialNum()
	local  accountID  =  Util.HashString(s.username)
	local  sessionID  =  userctx.sessionID;
	self.loginDatas:Add(serialNum,accountID,userctx.sessionID)

	Log.Trace("2.Login username[%s] password[%s] accountID[%u] sessionID[%d] add to login data ", s.username,s.password ,accountID,sessionID)

	-- send to login module
	local smsg,smsgw = SerializeUtil.SerializeEx(MsgID.MSG_C2S_REQ_LOGIN)
	smsgw:WriteUint64(serialNum)
	smsgw:WriteUint64(accountID)
	smsgw:WriteString(s.password)

	if self.loginModule == 0 then
		self.loginModule = thisModule:GetOtherModule("login")
	end

	assert(thisModule:GetLoginModule() ~= 0, "can not find login module")

	thisModule:SendRPC(thisModule:GetLoginModule(),smsg,  function (msg)
		Log.Trace("3.login receive echo accountID[%u] ", accountID);
		local ld = self.loginDatas:Find(serialNum)
		if nil == ld then
			Log.Warn("3.login receive echo accountID[%u], can not find logindatas", accountID);
			return
		end
	
		local mr = MessageReader.new(msg)
		local loginret = mr:ReadString()
		local sn = mr:ReadUint64()
		local act = mr:ReadUint64()


		assert(serialNum == sn and  accountID == act,"login check")

		if loginret == "OK" then

			Log.Trace("4.login find login data accountID[%u] ", act);

			self.connects:SetAccount(ld.sessionID, act)

			Log.Trace("5.login add Connection data sessionID:%u accountID%u]  ", ld.sessionID, act);
			Log.Trace("6.login success [accountID %u]  ", act);
		end

		local s2clogin = { ret = "AccountNotExsit",accountID = act }
		local s2cmsg = SerializeUtil.Serialize(MsgID.MSG_S2C_LOGIN_RESULT,"NetMessage.S2CLogin",s2clogin)

		thisModule:SendNetMessage(sessionID,s2cmsg)

		if self.loginDatas:Remove(serialNum) then
			Log.Trace("7.login remove login data [%u] success", serialNum);
		end
	end )
end

function GateLoginHandler:OnSetPlayerID(userctx,data)
	local accountID = data:ReadUint64()
	local playerID = data:ReadUint64()

	local conn = self.connects:FindByAccount(accountID)
	if nil == conn then
		Log.ConsoleError("OnSetPlayerID:can not find Connection.");
	end

	self.connects:SetPlayer(accountID,playerID)
	Log.Trace("set accountid[%u] playerid:%u",accountID,playerID)
end

function GateLoginHandler:OnSetSceneID(userctx,data)
	local playerID = data:ReadUint64()
	local sceneID = data:ReadUint64()

	local conn = self.connects:FindByPlayer(playerID)
	if nil == conn then
		Log.ConsoleError("OnSetSceneID:can not find Connection.");
	end

	conn.sceneID = sceneID
end

return GateLoginHandler

