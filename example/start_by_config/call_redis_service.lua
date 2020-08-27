local moon = require("moon")

local function send(db, ...)
    moon.send("lua", db, "send", 1, ...)
end

local function call(db, ...)
    return moon.co_call("lua", db, ...)
end

moon.start(function()
    local count = 0
    local db = moon.queryservice("redis")

    send(db,"del","C")
    send(db,"set", "A", "hello")
    send(db,"set", "B", "world")
    send(db,"sadd", "C", "one")

    moon.async(function()
        print(call(db, "get", "A"))
        print(call(db, "get", "B"))
        print(call(db, "sadd", "C", "two"))
    end)

    moon.async(function()
        local index = 1
        while true do
            -- call(db, "get", "dog")
            local res,err = call(db, "set", "dog"..tostring(index), "an animaldadadaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")
            if not res then
                print("redis set error", res, err)
                moon.sleep(1000)
            else
                count=count+1
                index = index+1
            end
        end
    end)

    moon.repeated(1000,-1, function()
        print(count)
        count = 0
    end)
end)