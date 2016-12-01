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

function LoginDatas:Size()
	local n1 = 0
    for i,v in pairs(self.logindatas) do
		n1 = n1+1
	end

	local n2 = 0
    for i,v in pairs(self.sessions) do
		n2 = n2+1
	end

	return n1,n2
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