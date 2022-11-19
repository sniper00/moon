---@meta

error("DO NOT REQUIRE THIS FILE")

---@class random
local random = {}

---integer rand range [min,max]
---@param min integer
---@param max integer
---@return integer
function random.rand_range(min, max) end

---integer rand range [min,max],返回范围内指定个数不重复的随机数
---@param min integer
---@param max integer
---@param count integer
---@return table
function random.rand_range_some(min, max, count) end

---float rand range [min,max)
---@param min number
---@param max number
---@return number
function random.randf_range(min, max) end

---参数 percent (0,1.0),返回bool
---@param percent number
---@return boolean
function random.randf_percent(percent) end

---按权重随机
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@return integer
function random.rand_weight(values, weights) end

---按权重随机count个值
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@param count integer @要随机的个数
---@return table
function random.rand_weight_some(values, weights, count) end

return random