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

function Connects:SetAccount(session, account)
    local conn = { }
    conn.sessionID = session
    conn.accountID = account
    conn.playerID = 0
    conn.sceneID = 0
    assert(self.connections[session] == nil)
    self.connections[session] = conn
    assert(self.accounts[account] == nil)
    self.accounts[account] = conn
end

function Connects:SetPlayer(account, player)
    local conn = self.accounts[account]
    if nil ~= conn then
        conn.playerID = player
        self.players[player] = conn
    end
end

function Connects:Remove(session)
    local conn = self.connections[session]
    if nil ~= conn then
        self.accounts[conn.accountID] = nil
        self.players[conn.playerID] = nil
        self.connections[session] = nil
        return true
    end
     return false
end

function Connects:RemoveByPlayer(player)
    local conn = self.players[player]
    if nil ~= conn then
        self.accounts[conn.accountID] = nil
        self.connections[conn.sessionID] = nil
        self.players[player] = nil
        return true
    end
    return false
end

function Connects:RemoveByAccount(account)
    local conn = self.accounts[account]
    if nil ~= conn then
        self.connections[conn.sessionID] = nil
        self.players[conn.playerID] = nil
        self.accounts[account] = nil
        return true
    end
   return false
end

function Connects:Find(session)
    return self.connections[session]
end

function Connects:FindByAccount(account)
    return  self.accounts[account]
end

function Connects:FindByPlayer(player)
    return   self.players[player]
end

return Connects
