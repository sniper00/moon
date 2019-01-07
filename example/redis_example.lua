local moon = require("moon")
local log = require("moon.log")
local redis_pool = require("moon.db.redispool")

local function run_example()
    local pool = redis_pool.new()
    local n = 0

    local timerid = moon.repeated(1000,-1,function ()
        print("per sec ",n,pool:size())
        n = 0
    end)

    local function check_error(ret,err)
        if not ret then
            print(err)
            return
        end
        print(ret)
    end

    moon.async(function()
        local redis,err = pool:spawn()
        if not redis then
            print("error",err)
            return
        end

        local ret, err = redis:hset("hdog", 1, 999)
        print("redis pool create1",ret, err)
        check_error(ret,err)
        ret, err = redis:hget("hdog", 1)
        check_error(ret,err)
        assert(tonumber(ret)==999)
        ret, err = redis:set("dog", "an animal")
        check_error(ret,err)
        ret, err = redis:get("dog")
        check_error(ret,err)
        print(ret)
        ret, err =redis:hmset("myhash","field1","hello","field2", "World")
        check_error(ret,err)
        ret, err =redis:hmget("myhash", "field1", "field2", "nofield")
        check_error(ret,err)
        log.dump(ret)
    end)

    moon.async(function()

        local redis,err = pool:spawn()
        if not redis then
            print(err)
            return
        end

        redis:hmset("myhash","name","Ben","age",10)

        redis:hmset("myhash2",{name="ben",age=100})

        repeat
            moon.co_wait(1000)

            redis:init_pipeline()
            redis:set("cat", "Marry")
            redis:set("horse", "Bob")
            redis:get("cat")
            redis:get("horse")
            local results, err = redis:commit_pipeline()
            if not results then
                print("failed to commit the pipelined requests: ", err)
                break
            end

            for i, res in ipairs(results) do
                if type(res) == "table" then
                    if res[1] == false then
                        print("failed to run command ", i, ": ", res[2])
                    else
                        -- process the table value
                        log.dump(res)
                    end
                else
                    -- process the scalar value
                    print(res)
                end
            end
        until(false)
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
            redis.get("dog")
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