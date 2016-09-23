require("functions")
local Component = require("Component")

local AccountDatas = class("AccountDatas",Component)

function AccountDatas:ctor()
	AccountDatas.super.ctor(self)
	self.accounts = {}
	Log.Trace("ctor AccountDatas")
end

function AccountDatas:IsExistAccount( accountID )
	return self.accounts[accountID] ~= nil
end


function AccountDatas:FindAccountInfo( accountID )
	return self.accounts[accountID]
end


function AccountDatas:Match( accountID , password)
	local a = self:FindAccountInfo(accountID)
	if a ~= nil then
		return a.password == password
	end
	return false
end


function AccountDatas:IsOnline( accountID )
	local a = self:FindAccountInfo(accountID)
	if a ~= nil then
		if a.bOnline then
		end
		
		return a.bOnline
	end
	return false
end


function AccountDatas:OnLine( accountID )
	local a = self:FindAccountInfo(accountID)
	if a ~= nil then
		a.bOnline = true
		return true
	end
	return false
end


function AccountDatas:OffLine( accountID )
	local a = self:FindAccountInfo(accountID)
	if a ~= nil then
		a.bOnline = false
		return true
	end
	return false
end


function AccountDatas:AddAccount( accountID ,username, password)
	if self:IsExistAccount(accountID) then
		return false
	end

	local AccountData a = {}
	a.username = username
	a.password = password
	a.bOnline = false
	a.accountID = accountID

	self.accounts[accountID] = a

	return true
end

return AccountDatas

