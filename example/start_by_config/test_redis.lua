local moon = require("moon")
local redis = require("moon.db.redis")
local test_assert = require("test_assert")

local function run_test()
    moon.async(function()
        local db,err = redis.connect({host="127.0.0.1",port=6379})
        test_assert.assert(db, err)

        local res, err = db:set("set_key", "Hello World")
        test_assert.equal(res, "OK")

        res, err = db:get("set_key")
        test_assert.equal(res, "Hello World")

        res, err = db:set("set_key", "Hello World", "NX")
        test_assert.equal(res, nil)

        res, err = db:mset("mset_key1", 1, "mset_key2","2")
        test_assert.equal(res, "OK")

        res, err = db:mget("mset_key1","mset_key2")
        test_assert.linear_table_equal(res, {"1","2"})

        res, err = db:del("set_key")
        test_assert.equal(res, 1)

        res, err = db:incr("mset_key1")
        test_assert.equal(res, 2)

        res, err = db:decr("mset_key1")
        test_assert.equal(res, 1)

        res, err = db:hmset("mset_key3","field1","value1","field2","value2")
        test_assert.equal(res, "OK")

        res, err = db:hgetall("mset_key3")
        test_assert.linear_table_equal(res, {"field1","value1","field2","value2"})

        db:zadd("salary", 2500, "jack")
        db:zadd("salary", 500, "tom")
        db:zadd("salary", 12000, "peter")
        res, err = db:zrangebyscore("salary","-inf","+inf")
        test_assert.linear_table_equal(res, {"tom","jack","peter"})

        res, err = db:zrangebyscore("salary","-inf","+inf","WITHSCORES")
        test_assert.linear_table_equal(res, {"tom","500","jack","2500","peter","12000"})

        test_assert.success()
    end)
end

moon.start(function()
    run_test()
end)