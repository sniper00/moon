local moon = require("moon")
local random = require("random")
local zset = require("zset")

local max_count = 100000

local rank = zset.new(max_count)

local timestamp = 1

local bt = moon.clock()
for i=1,max_count do
    rank:update(i, random.rand_range(10000000,2000000000), timestamp)
    timestamp = timestamp + 1
end
print("init 1000000 cost", moon.clock() - bt)

for i=1,10 do
    bt = moon.clock()
    local uid = random.rand_range(10,max_count)
    rank:update(uid, random.rand_range(10000000,2000000000), timestamp)
    timestamp = timestamp + 1
    local nrank = rank:rank(uid)
    print("update one cost", moon.clock() - bt, nrank)
end







