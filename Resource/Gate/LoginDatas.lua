require("functions")

local Component = require("Component")

local LoginDatas = class("LoginDatas",Component)

function LoginDatas:ctor()
	LoginDatas.super.ctor(self)
	self.LoginIncreSerialNum = 0

	self.logindatas = {}
	self.sessions = {}

	Log.Trace("ctor LoginDatas")
end

function LoginDatas:CreateSerialNum()
	self.LoginIncreSerialNum  = self.LoginIncreSerialNum  +1
	return self.LoginIncreSerialNum 
end

function LoginDatas:Add(serialNum, accountID, sessionID)
	assert(self.logindatas[serialNum] == nil,"login data already exist")
	local d = {}
	d.serialNum = serialNum
	d.accountID = accountID
	d.sessionID = sessionID
	self.logindatas[serialNum] = d
	self.sessions[sessionID] = d
end

function  LoginDatas:Remove(serialNum)
	local d = self.logindatas[serialNum]
	if nil ~= d then
		self.logindatas[serialNum] = nil
		self.sessions[d.sessionID] = nil
		return true
	else
		return false
	end
end

function  LoginDatas:RemoveBySession(sessionID)
	local d = self.sessions[sessionID]
	if nil ~= d then
		self.logindatas[d.serialNum] = nil
		self.sessions[sessionID] = nil
		return true
	else
		return false
	end
end


function LoginDatas:Find( serialNum )
	return self.logindatas[serialNum] 
end

function LoginDatas:FindBySessionID(sessionID)
	return self.sessions[sessionID]
end

return LoginDatas