local moon = require("moon")
---@type redis_client
local redis = require("redisd")

local count = 0
local db = moon.queryservice("redis")

redis.send(db,"del","C")
redis.send(db,"set", "A", "hello")
redis.send(db,"set", "B", "world")
redis.send(db,"sadd", "C", "one")

moon.async(function()
    print(redis.call(db, "get", "A"))
    print(redis.call(db, "get", "B"))
    print(redis.call(db, "sadd", "C", "two"))
end)

moon.async(function()
    local index = 1
    while true do
        -- call(db, "get", "dog")
        local res,err = redis.call(db, "set", "dog"..tostring(index), "an animaldadadaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")
        if not res then
            print("redis set error", res, err)
            moon.sleep(1000)
        else
            count=count+1
            index = index+1
        end
    end
end)

moon.async(function()
    while true do
        moon.sleep(1000)
        print(count)
        count = 0
    end
end)
