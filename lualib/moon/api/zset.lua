error("DO NOT REQUIRE THIS FILE")

---@class zset
local zset = {}

---@param maxcount integer @排行榜上限
---@return zset
function zset.new(maxcount)

end

---只排序O(logN)
---@param key integer @排行对象唯一ID
---@param score integer @积分
---@param timestamp integer @积分相同按时间戳排序
function zset:update(key, score, timestamp)

end

---处理排行O(N)
function zset:prepare()

end

---获取排行, 0 未上榜. 会自动调用prepare
---@param key integer @排行对象唯一ID
---@return integer
function zset:rank(key)

end

---获取位于排行nrank的对象唯一ID. 会自动调用prepare
---@param nrank integer
---@return integer
function zset:key(nrank)

end


---获取积分
---@param key integer @排行对象唯一ID
---@return integer
function zset:score(key)

end

---是否在排行榜中
---@param key integer @排行对象唯一ID
---@return boolean
function zset:has(key)

end

---排行榜实际大小
function zset:size()

end

function zset:clear()

end

---删除某个对象
---@param key integer @排行对象唯一ID
---@return integer @删除的个数
function zset:erase(key)

end

---获取[start, finish] 对象唯一ID 数组
---@param start integer
---@param finish integer
---@return table
function zset:range(start, finish)

end

return zset