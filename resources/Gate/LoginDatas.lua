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

function LoginDatas:Add(serialnum, accountid, sessionid)
	assert(self.logindatas[serialNum] == nil,"login data already exist")
	local d = {}
	d.serialnum = serialnum
	d.accountid = accountid
	d.sessionid = sessionid
	self.logindatas[serialnum] = d
	self.sessions[sessionid] = d
end

function  LoginDatas:Remove(serialnum)
	local d = self.logindatas[serialnum]
	if nil ~= d then
		self.logindatas[serialnum] = nil
		self.sessions[d.sessionid] = nil
		return true
	else
		return false
	end
end

function  LoginDatas:RemoveBySession(sessionid)
	local d = self.sessions[sessionid]
	if nil ~= d then
		self.logindatas[d.serialnum] = nil
		self.sessions[sessionid] = nil
		return true
	else
		return false
	end
end


function LoginDatas:Find( serialnum )
	return self.logindatas[serialnum] 
end

function LoginDatas:FindBySessionID(sessionid)
	return self.sessions[sessionid]
end

return LoginDatas