local moon = require("moon")
---@type random
local random = require("random")

local counter = {}
for i=1,10000 do
    local v = {11,12,13,14,15}
    local w = {100,200,300,400,500}
    local res = random.rand_weight_some(v, w, 1)
    local n = counter[res[1]] or 0
    counter[res[1]] = n + 1
end

for k,v in pairs(counter) do
    print(k, string.format("%.02f", (v/10000)*100))
end

print(random.rand_range(1,10))
print("rand_range_some", table.concat(random.rand_range_some(1,10,2)," "))

print(random.rand_weight({1,2,3,4,5},{100,200,300,400,500}))
print(random.rand_weight_some({1,2,3,4,5},{100,200,300,400,500},1))
print(random.randf_percent(0.8))

print(pcall(random.rand_range, 1))
print(pcall(random.rand_range, nil, 10))
print(pcall(random.randf_percent, 0.8))

print(pcall(random.rand_range_some, 1))
print(pcall(random.rand_range_some, 1, 10))
print(pcall(random.rand_range_some, nil, 10, 2))
print(pcall(random.rand_range_some, 1, nil, 2))

print(pcall(random.rand_weight, {1,2,3,4,5},{100,200,300,400}))
-- random.rand_weight_some({1,2,3,4,5},{100,200,300,400},5)
print(pcall(random.rand_weight_some, {1,2,3,4,5},{100,200,300,400},5))


