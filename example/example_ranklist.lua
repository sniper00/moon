local moon = require("moon")
local core_ranklist = require("ranklist_core")
---@type random
local random = require("random")
---@type ranklist
local ranklist = core_ranklist.ranklist

---@type ranklist
local employ = ranklist.new(-1)

---插入10000条信息
local start = moon.microsecond()
for i=1, 100 do
    employ:update(i, random.rand_range(1, 10000), random.rand_range(1000000, 100000000))
end
print("Init RankList 10000, cost:", moon.microsecond() - start)
print("rank size:", employ:size())

local testele = {id=1000001, score=10000001, time = 1}
start = moon.microsecond()
local uprank = employ:update(testele.id, testele.score, testele.time)
print("update cost time:", moon.microsecond() - start, ",rank:", uprank)
---打印前十名
employ:print(1, 10)
start = moon.microsecond()
---获取testid的排名信息
local id,score, time = employ:uniqueid_rankinfo(testele.id)
local cost = moon.microsecond() - start;
print(string.format("get %d cost %d, info %d %d %d", testele.id, cost, id, score, time))
---获取testid的排名
local testrank = employ:uniqueid_rank(testele.id)
print(string.format("id %d ranks %d", testele.id, testrank))
---获取第一名uniqueid
print("rank 1 id:", employ:rank_uniqueid(1))
---获取前10名信息
local top10 = employ:ranks_uniqueids(1, 10)
print("top10:\n")
print_r(top10)
---清除
employ:clear()
print("after clear, ranklist size:", employ:size())




