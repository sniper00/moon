error("DO NOT REQUIRE THIS FILE")

---@class uuid
local uuid = {}

---初始化,只用调用一次
---@param serverid integer
---@param server_start_times integer
---@param servergroup integer
---@param region integer
function uuid.init(serverid, server_start_times, servergroup, region)

end

---生成 int64 uuid,可以传入类型，最大值 1023
---@param type_ integer
---@return integer
function uuid.next(type_)

end

---获取uuid的 type
---@param uuidvalue integer
---@return integer
function uuid.type(uuidvalue)

end

---生成 int64 player uid
---@return integer
function uuid.allocuid()
end

return uuid