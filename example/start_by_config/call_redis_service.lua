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
    local json = require("json")

    local args = {}
    for i=1,1000,2 do
        args[i] = i
        args[i+1] = i
    end

    local res,err = redis.call(db, "hmset", "test", table.unpack(args))
    if not res then
        print("redis set error", res, err)
        moon.sleep(1000)
    else
        count=count+1
    end

    local t,err = redis.call(db, "hgetall", "test")
    for i=1,#t,2 do
        assert( math.tointeger(t[i]) == math.tointeger(t[i+1]))
    end
end)

moon.async(function()
    while true do
        moon.sleep(1000)
        print(count)
        count = 0
    end
end)
