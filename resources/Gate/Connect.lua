require("functions")
local Component = require("Component")

local Connects = class("Connects", Component)

function Connects:ctor()
    Connects.super.ctor(self)

    self.connections = { }
    self.accounts = { }
    self.players = { }

    Log.Trace("ctor Connects")
end

function Connects:Size()
    local n1 = 0
    for i,v in pairs(self.connections) do
        n1 = n1+1
    end

    local n2 = 0
    for i,v in pairs(self.accounts) do
        n2 = n2+1
    end

    local n3 = 0
    for i,v in pairs(self.players) do
        n3 = n3+1
    end

    return n1,n2,n3
end

function Connects:SetAccount(sessionid, accountid)
    local conn = { }
    conn.sessionid = sessionid
    conn.accountid = accountid
    conn.playerid = 0
    conn.sceneid = 0
    assert(self.connections[sessionid] == nil)
    self.connections[sessionid] = conn
    assert(self.accounts[accountid] == nil)
    self.accounts[accountid] = conn
end

function Connects:IsLogined(sessionid)
    return self.connections[sessionid] ~=nil
end

function Connects:SetPlayer(account, player)
    local conn = self.accounts[account]
    if nil ~= conn then
        conn.playerid = player
        self.players[player] = conn
    end
end

function Connects:Remove(session)
    local conn = self.connections[session]
    if nil ~= conn then
        self.accounts[conn.accountid] = nil
        self.players[conn.playerid] = nil
        self.connections[session] = nil
        return true
    end
    return false
end

function Connects:RemoveByPlayer(player)
    local conn = self.players[player]
    if nil ~= conn then
        self.accounts[conn.accountid] = nil
        self.players[player] = nil
        return true
    end
    return false
end

function Connects:RemoveByAccount(account)
    local conn = self.accounts[account]
    if nil ~= conn then
        self.players[conn.playerid] = nil
        self.accounts[account] = nil
        return true
    end
   return false
end

function Connects:Find(session)
    return self.connections[session]
end

function Connects:FindByAccount(account)
    return self.accounts[account]
end

function Connects:FindByPlayer(player)
    return self.players[player]
end

return Connects
