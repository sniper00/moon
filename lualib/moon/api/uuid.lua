---@meta

error("DO NOT REQUIRE THIS FILE")

---@class uuid
local uuid = {}

---Init global config
---@param channel integer
---@param serverid integer
---@param boottimes integer
function uuid.init(channel, serverid, boottimes) end

---Generate uuid
---@param type integer?
---@return integer
function uuid.next(type) end

---Get type of uuid
---@param uid integer
---@return integer
function uuid.type(uid) end

---Get serverid of uuid
---@param uid integer
---@return integer
function uuid.serverid(uid) end

---Check value is uuid
---@param uid integer
---@return boolean
function uuid.isuid(uid) end

return uuid
