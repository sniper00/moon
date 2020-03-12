local moon = require("moon")
---@type random
local random = require("random")


for i=1,100 do
    -------------------------[min,max]
    print("rand_range", random.rand_range(1,10))
end

for i=1,100 do
    -------------------------[min,max]
    print("rand_range_some", table.concat(random.rand_range_some(1,10,2)," "))
end

local val_weight = {
    [101] = 100,
    [102] = 50,
    [103] = 60,
    [104] = 110,
    [110] = 200,
    [111] = 300,
    [112] = 100,
    [113] = 200,
    [114] = 100,
    [115] = 500,
}

for i=1,100 do
    print("rand_weight use hash table", random.rand_weight(val_weight))
end

local res = {}

for i=1,100000 do
    local v = random.rand_weight(val_weight)
    if not res[v] then
        res[v] = 1
    else
        res[v] = res[v] + 1
    end
end

for k,v in pairs(res) do
    local percent = (v/100000)*100
    print(k,percent,"%")
end

for i=1,100 do
    print("rand_weight_some use hash table", table.concat(random.rand_weight_some(val_weight, 6)," "))
end

local val = {101,102,103,104,110,111,112,113,114}
local rad = {100,50,60,110,200,20,40,50,100}

for i=1,100 do
    print("rand_weight use array", random.rand_weight(val, rad))
end

for i=1,100 do
    print("rand_weight_some use array", table.concat(random.rand_weight_some(val, rad, 9)," "))
end
