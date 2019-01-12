local moon = require("moon")
local redis_pool = require("moon.db.redispool")
local test_assert = require("test_assert")

local function run_test()
    local pool = redis_pool.new()
    test_assert.equal(pool:size(), 0)

    moon.async(function()
        local redis,err = pool:spawn()
        test_assert.assert(redis, err)

        local res, err = redis:set("set_key", "Hello World")
        test_assert.equal(res, "OK")

        res, err = redis:get("set_key")
        test_assert.equal(res, "Hello World")

        res, err = redis:set("set_key", "Hello World", "NX")
        test_assert.equal(res, moon.null)

        res, err = redis:mset("mset_key1", 1, "mset_key2","2")
        test_assert.equal(res, "OK")

        res, err = redis:mget("mset_key1","mset_key2")
        test_assert.linear_table_equal(res, {"1","2"})

        res, err = redis:del("set_key")
        test_assert.equal(res, 1)

        res, err = redis:incr("mset_key1")
        test_assert.equal(res, 2)

        res, err = redis:decr("mset_key1")
        test_assert.equal(res, 1)

        redis:zadd("salary", 2500, "jack")
        redis:zadd("salary", 500, "tom")
        redis:zadd("salary", 12000, "peter")
        res, err = redis:zrangebyscore("salary","-inf","+inf")
        test_assert.linear_table_equal(res, {"tom","jack","peter"})

        res, err = redis:zrangebyscore("salary","-inf","+inf","WITHSCORES")
        test_assert.linear_table_equal(res, {"tom","500","jack","2500","peter","12000"})

        test_assert.success()
    end)
end

moon.start(function()
    run_test()
end)