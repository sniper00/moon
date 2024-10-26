local moon = require("moon")
local random = require("random")
local zset = require("zset")
local test_assert = require("test_assert")

local max_count = 100000

local item = {}
local item2 = {}

for i=1,max_count do
    local score = random.rand_range(10000000,2000000000)
    item[#item+1] = {i, score}
    item2[#item2 + 1] = {i, score}
end

table.sort(item2, function(a, b)
    if a[2] == b[2] then
        return a[1]<b[1]
    end
    return a[2]>b[2]
end)

local rank = zset.new(max_count)

local timestamp = 1

local bt = moon.clock()
for i=1,max_count do
    local one = item[i]
    rank:update(one[1], one[2], timestamp)
end
print("init ",max_count, "cost", moon.clock() - bt)

for i=1,max_count do
    local order = item2[i]
    local key1 = rank:key_by_rank(i)
    local key2 = order[1]
    --print(i, key1, key2, rank:size())
    assert(rank:key_by_rank(i) == order[1], string.format("%d %d", key1, key2))
end

for i=1,10 do
    bt = moon.clock()
    local uid = random.rand_range(10,max_count)
    rank:update(uid, random.rand_range(10000000,2000000000), timestamp)
    timestamp = timestamp + 1
    local nrank = rank:rank(uid)
    print("update one cost", moon.clock() - bt, nrank)
end

rank:clear()

local bt = moon.clock()
for i=1,max_count do
    local one = item[i]
    rank:update(one[1], one[2], timestamp)
end
print("init ",max_count, " cost", moon.clock() - bt)

for i=1,max_count do
    local order = item2[i]
    local key1 = rank:key_by_rank(i)
    local key2 = order[1]
    assert(rank:key_by_rank(i) == order[1], string.format("%d %d", key1, key2))
end

for i=1,10 do
    bt = moon.clock()
    local uid = random.rand_range(10,max_count)
    rank:update(uid, random.rand_range(10000000,2000000000), timestamp)
    timestamp = timestamp + 1
    local nrank = rank:rank(uid)
    print("update one cost", moon.clock() - bt, nrank)
end

collectgarbage("collect")

do
    ---test max count
    local rank = zset.new(2)
    rank:update(1,100,1)
    rank:update(2,200,1)
    rank:update(3,300,1)
    assert(rank:size()==2)
    assert(rank:rank(3) == 1)
    assert(rank:rank(2) == 2)
    assert(rank:rank(1) == nil)

    assert(rank:score(1) == 0)
    assert(rank:score(2) == 200)
    assert(rank:score(3) == 300)

    rank:erase(3)
    ---insert key = 1
    rank:update(1,100,1)

    assert(rank:size()==2)
    assert(rank:rank(3) == nil)
    assert(rank:rank(2) == 1)
    assert(rank:rank(1) == 2)
end

do
    ---test max count
    --reverse
    local rank = zset.new(3, true)
    rank:update(1,100,1)
    rank:update(2,200,1)
    rank:update(3,300,1)
    rank:update(4,400,1)
    assert(rank:size()==3)
    assert(rank:rank(1) == 1)
    assert(rank:rank(2) == 2)
    assert(rank:rank(3) == 3)
    assert(rank:rank(4) == nil) ---no rank
end

do
    --test range

    local rank = zset.new(4)
    rank:update(11,100,1)
    rank:update(21,200,1)
    rank:update(31,300,1)
    rank:update(41,400,1)

    local t = rank:range(1,2)
    assert(t[1] == 41)
    assert(t[2] == 31)
    assert(#t == 2)

    t = rank:range(1,2, true)
    assert(t[1] == 11)
    assert(t[2] == 21)
    assert(#t == 2)

    t = rank:range(1,100)
    assert(t[1] == 41)
    assert(t[2] == 31)
    assert(t[3] == 21)
    assert(t[4] == 11)
    assert(#t == 4)
end

do
    ---test get key by rank
    local rank = zset.new(4)
    rank:update(1,100,1)
    rank:update(2,200,1)
    rank:update(3,300,1)
    rank:update(4,400,1)

    assert(rank:key_by_rank(1) == 4)
    assert(rank:key_by_rank(2) == 3)
    assert(rank:key_by_rank(3) == 2)
    assert(rank:key_by_rank(4) == 1)

    ---delete key==2 rank==3
    rank:erase(2)

    assert(rank:key_by_rank(1) == 4)
    assert(rank:key_by_rank(2) == 3)
    assert(rank:key_by_rank(3) == 1)
end

do
    ---test get rank by key
    local rank = zset.new(3)
    rank:update(1,100,1)
    rank:update(2,200,1)
    rank:update(3,300,1)
    rank:update(4,400,1)

    assert(rank:rank(1) == nil)
    assert(rank:rank(2) == 3)
    assert(rank:rank(3) == 2)
    assert(rank:rank(4) == 1)
end

test_assert.success()





