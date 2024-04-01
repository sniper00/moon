local moon = require("moon")
local redis = require("moon.db.redis")

local HOST = "127.0.0.1"
local PORT = 6379

local function run_example()

    local n = 0
    moon.async(function()
        while true do
            moon.sleep(1000)
            print("per sec ",n)
            n = 0
        end
    end)

    moon.async(function()
        local db,err = redis.connect({host = HOST,port = PORT})
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

        db:disconnect()
    end)

    moon.async(function()

        local db,err = redis.connect({host = HOST,port = PORT})
        if not db then
            print(err)
            return
        end

        db:hmset("myhash","name","Ben","age",11)

        repeat
            moon.sleep(1000)

            local res, err = db:pipeline({
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
        db:disconnect()
    end)

    moon.async(function()
        local db,err = redis.connect({host = HOST,port = PORT})
        if not db then
            print(err)
            return
        end

        local index = 1
        while true do
            local ret,errmsg = db:set("dog"..tostring(index), "an animaldadadaddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd")
            if not ret then
                print("set", errmsg)
                break
            end
            assert ('OK'== ret,ret)
            n=n+1
            index = index+1
        end
        db:disconnect()
    end)

    local redisd = require("redisd")
    --初始化服务配置
    local db_conf= {host = HOST, port = PORT, timeout = 1000}

    local redis_db

    moon.async(function ()
        redis_db = moon.new_service(    {
            unique = true,
            name = "db",
            file = "../service/redisd.lua",
            threadid = 1, ---独占线程
            poolsize = 5, ---连接池
            opts = db_conf
        })

        redisd.send(redis_db, "SET", "HELLO", "WORLD")
        print("Get HELLO:", redisd.call(redis_db, "GET", "HELLO"))

        redisd.send(redis_db, "SET", "A", "A1")
        redisd.send(redis_db, "SET", "B", "B1")
        redisd.send(redis_db, "SET", "C", "C1")
        print(redisd.call(redis_db, "GET", 'A'))
        print(redisd.call(redis_db, "GET", 'B'))
        print(redisd.call(redis_db, "GET", 'C'))

        local res, err = redisd.direct(redis_db, "pipeline", {{"set", "cat", "Marry"},
            {"set", "horse", "Bob"},
            {"get", "cat"},
            {"get", "horse"},
            },{});

        if err then
            print(err)
        else
            print_r(res)
        end
    end)

    moon.shutdown(function ()
        moon.kill(redis_db)
        moon.quit()
    end)
end

run_example()