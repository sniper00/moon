local moon = require("moon")
local redis_pool = require("moon.db.redispool")

local function run_example()
    local pool = redis_pool.new()
    local n = 0

    local timerid = moon.repeated(1000,-1,function ()
        print("per sec ",n,pool:size())
        n = 0
    end)

    moon.async(function()
        local db,err = pool:spawn()
        if not db then
            print("error",err)
            return
        end

        db:del "C"
        db:set("A", "hello")
        db:set("B", "world")
        db:sadd("C", "one")

        print(db:get("A"))
        print(db:get("B"))

        db:del "D"
        for i=1,10 do
            db:hset("D",i,i)
        end
        local r = db:hvals "D"
        for k,v in pairs(r) do
            print(k,v)
        end

        db:multi()
        db:get "A"
        db:get "B"
        local t = db:exec()
        for k,v in ipairs(t) do
            print("Exec", v)
        end

        print(db:exists "A")
        print(db:get "A")
        print(db:set("A","hello world"))
        print(db:get("A"))
        print(db:sismember("C","one"))
        print(db:sismember("C","two"))

        print("===========publish============")

        for i=1,10 do
            db:publish("foo", i)
        end
        for i=11,20 do
            db:publish("hello.foo", i)
        end

        pool:close(db)
    end)

    moon.async(function()

        local redis,err = pool:spawn()
        if not redis then
            print(err)
            return
        end

        redis:hmset("myhash","name","Ben","age",11)

        repeat
            moon.co_wait(1000)

            local res, err = redis:pipeline({
                {"set", "cat", "Marry"},
                {"set", "horse", "Bob"},
                {"get", "cat"},
                {"get", "horse"},
                },{}) -- offer a {} for result

            if err then
                print(err)
            else
                print_r(res)
            end

        until(true)
        pool:close(redis)
    end)

    moon.async(function()
        local redis,err = pool:spawn()
        if not redis then
            print("error",err)
            return
        end
        local index = 1
        while true do
            local ret,errmsg = redis:set("dog"..tostring(index), "an animaldadadaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")
            if not ret then
                print("set", errmsg)
                moon.remove_timer(timerid)
                break
            end
            assert ('OK'== ret,ret)
            n=n+1
            index = index+1
        end
        pool:close(redis)
    end)

end

moon.start(function()
    run_example()
end)