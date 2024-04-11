local moon = require("moon")
local json = require "json"
local pg = require("moon.db.pg")
local buffer = require "buffer"

local db_config = {
    host = "127.0.0.1",
    port = 5432,
    database = "postgres",
    user = "postgres",
    password = "123456", -- only support md5 auth
    connect_timeout = 1000,
}

local function test_json_query()
    local db = pg.connect(db_config)

    if db.code then
        moon.error(print_r(db, true))
        return
    end

    ------------------------------json data---------------------------------

    db:query("drop table if exists t_track")
    db:query("CREATE TABLE t_track ( a jsonb)")

    db:query([[
        INSERT INTO t_track (a) VALUES ('
        {
            "gpsname": "gps1",
            "track": {
                "segments": [
                    {
                        "location": [
                            47.763,
                            13.4034
                        ],
                        "start time": "2018-10-14 10:05:14",
                        "HR": 73
                    },
                    {
                        "location": [
                            47.706,
                            13.2635
                        ],
                        "start time": "2018-10-14 10:39:21",
                        "HR": 130
                    }
                ]
            }
        }');
    ]])

    print_r(db:query("SELECT jsonb_pretty(a) FROM t_track;"))

    print_r(db:query("SELECT jsonb_path_query(a, '$.track.segments[0]') FROM t_track;"))



    -- {
    --     data = {
    --         [1] = {
    --             jsonb_path_query   = "{"HR": 73, "location": [47.763, 13.4034], "start time": "2018-10-14 10:05:14"}",
    --         },
    --     },
    --     num_queries   = 1,
    -- }

    print_r(db:query("SELECT jsonb_path_query(a, '$.track.segments ? (@.HR>100)') FROM t_track;"))
    -- {
    --     data = {
    --         [1] = {
    --             jsonb_path_query   = "{"HR": 130, "location": [47.706, 13.2635], "start time": "2018-10-14 10:39:21"}",
    --         },
    --     },
    --     num_queries   = 1,
    -- }
end

local function test_big_json_value()
    local db = pg.connect(db_config)

    if db.code then
        moon.error(print_r(db, true))
        return
    end

    db:query("drop table if exists game_user")
    print_r(db:query("CREATE TABLE game_user (uid bigint primary key, data text)"))

    local big_json_str = json.encode(json.decode(io.readfile("../test/twitter.json")))

    local sql = string.format(
        "INSERT INTO game_user (uid, data) VALUES (1, '%s') on conflict (uid) do update set data = excluded.data",
        big_json_str)


    local bt = moon.clock()
    for i = 1, 100 do
        local res = db:query(sql)
        assert(not res.code, res.message)
    end

    print("big json cost", (moon.clock() - bt) / 100)
end

local function test_jsonb()
    local db = pg.connect(db_config)

    if db.code then
        moon.error(print_r(db, true))
        return
    end

    db:query("drop table if exists game_user")
    db:query("CREATE TABLE game_user (uid bigint primary key, data jsonb)")

    local sql0 = [[
        insert into game_user(uid, data) VALUES(1, '%s') on conflict(uid) do update set data = EXCLUDED.data;
    ]]

    sql0 = string.format(sql0, io.readfile("../test/twitter.json"))

    print_r(db:query(sql0))

    local sql = [[
        BEGIN;
        update game_user set data = jsonb_set(data, '{user,id}', to_jsonb(15), false) where uid = 1;
        COMMIT;
    ]]

    local res
    local bt = moon.clock()
    for i = 1, 100 do
        res = db:query(sql)
        assert(not res.code, res.message)
    end
    print("jsonb cost", (moon.clock() - bt)/100)
end


local jsonstr = [[
    {
        "gpsname": "gps1",
        "track": {
            "segments": [
                {
                    "location": [
                        47.763,
                        13.4034
                    ],
                    "start time": "2018-10-14 10:05:14",
                    "HR": 73
                },
                {
                    "location": [
                        47.706,
                        13.2635
                    ],
                    "start time": "2018-10-14 10:39:21",
                    "HR": 130
                }
            ]
        }
    }
]]

local simple_json_field = json.decode(jsonstr)

local function test_key_value_table_raw()
    local db = pg.connect(db_config)

    if db.code then
        moon.error(print_r(db, true))
        return
    end

    local sql = string.format([[
        --create userdata table
        drop table if exists userdata;
        create table userdata (
            uid	bigint,
            key		text,
            value   jsonb,
            CONSTRAINT pk_userdata PRIMARY KEY (uid, key)
           );
    ]])

    db:query(sql)

    local function make_one_row(t, uid, key, value)
        if type(value) == "string" then
            value = json.encode(value)
        end

        local n = #t
        t[n + 1] = "insert into userdata(uid, key, value) values("
        t[n + 2] = uid
        t[n + 3] = ",'"
        t[n + 4] = key
        t[n + 5] = "','"
        t[n + 6] = value
        t[n + 7] = "') on conflict (uid, key) do update set value = excluded.value;"
    end

    local user = {
        name = "hello",
        age = 10,
        level = 99,
        exp = 20,
        info1 = simple_json_field,
        info2 = simple_json_field,
    }

    local cache = {}
    cache[#cache + 1] = "BEGIN;"

    for key, value in pairs(user) do
        make_one_row(cache, 233, key, value)
    end

    cache[#cache + 1] = "COMMIT;"

    sql = buffer.unpack(json.concat(cache))

    -- print(sql)

    local bt = moon.clock()
    for m = 1, 100 do
        local res = db:query(sql)
        assert(not res.code, res.message)
    end

    print("test_key_value_table_raw insert cost", (moon.clock() - bt)/100)

    sql = string.format([[
        --select userdata
        select key, value from userdata where uid = %d;
    ]], 233)

    local res = db:query(sql)
    local data = res.data

    -- print_r(data)

    local user = {}
    for index, value in ipairs(data) do
        user[value.key] = json.decode(value.value)
    end

    -- print_r(user)
end

local function test_key_value_table_function()
    local fn_sql = [[
        CREATE OR REPLACE FUNCTION update_userdata(pk integer, VARIADIC key_values text[]) RETURNS void AS $$
DECLARE
    i integer;
BEGIN
    FOR i IN 1..array_length(key_values, 1) BY 2 LOOP
        INSERT INTO userdata(uid, key, value) VALUES (pk, key_values[i], key_values[i+1]::json) ON CONFLICT (uid, key) DO UPDATE SET value = excluded.value::json;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
    ]]

    local db = pg.connect(db_config)

    if db.code then
        moon.error(print_r(db, true))
        return
    end

    local sql = string.format([[
        --create userdata table
        drop table if exists userdata;
        create table userdata (
            uid	bigint,
            key		text,
            value   jsonb,
            CONSTRAINT pk_userdata PRIMARY KEY (uid, key)
           );
    ]])
    db:query(sql)

    local res = db:query(fn_sql)
    assert(not res.code, res.message)

    ---@class User
    ---@field name string
    ---@field age number
    ---@field level number
    ---@field exp number
    local user = {
        name = "hello",
        age = 10,
        level = 99,
        exp = 11,
        info1 = simple_json_field,
        info2 = simple_json_field,
    }

    local key_value_model = require "key_value_model"

    ---@type User Description
    local user = key_value_model.new("userdata", 233, user)

    user.age = 101
    user.level = 100
    user.info1.cc = 200
    user.info2.cc = 300
    user.name = "hello world"

    local sql = key_value_model.modifyed(user)

    -- print(sql)
    local bt = moon.clock()

    local res
    for m = 1, 100 do
        res = db:query(sql)
        assert(not res.code, res.message)
    end

    print("test_key_value_table_function insert cost", (moon.clock() - bt)/100)

    sql = string.format([[
        --select userdata
        select key, value from userdata where uid = %d;
    ]], 233)
    local res = db:query(sql)
    local data = res.data

    -- print_r(data)

    -- local user = {}
    -- for index, value in ipairs(data) do
    --     user[value.key] = json.decode(value.value)
    -- end
    -- print_r(user)

    -- select * from public.userdata where key='level' AND (value)::int = 100;
end

local function test_sql_driver()
    local sqldriver = require("service.sqldriver")
    local db = moon.new_service {
        unique = true,
        name = "db_game",
        file = "../service/sqldriver.lua",
        provider = "moon.db.pg",
        threadid = 2,
        poolsize = 5,
        opts = db_config
    }

    assert(db>0)

    local fn_sql = [[
        CREATE OR REPLACE FUNCTION update_userdata(pk integer, VARIADIC key_values text[]) RETURNS void AS $$
DECLARE
    i integer;
BEGIN
    FOR i IN 1..array_length(key_values, 1) BY 2 LOOP
        INSERT INTO userdata(uid, key, value) VALUES (pk, key_values[i], key_values[i+1]::json) ON CONFLICT (uid, key) DO UPDATE SET value = excluded.value::json;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
    ]]

    local sql = string.format([[
        --create userdata table
        drop table if exists userdata;
        create table userdata (
            uid	bigint,
            key		text,
            value   jsonb,
            CONSTRAINT pk_userdata PRIMARY KEY (uid, key)
           );
    ]])

    print(1)
    sqldriver.query(db, sql)
    print(2)

    local res = sqldriver.query(db, fn_sql)
    assert(not res.code, res.message)

    ---@class User
    ---@field name string
    ---@field age number
    ---@field level number
    ---@field exp number
    local user = {
        name = "hello",
        age = 10,
        level = 99,
        exp = 11,
        info1 = simple_json_field,
        info2 = simple_json_field,
    }

    local key_value_model = require "key_value_model"

    ---@type User Description
    local user = key_value_model.new("userdata", 233, user)

    user.age = 101
    user.level = 100
    user.info1.cc = 200
    user.info2.cc = 300
    user.name = "hello world"

    local sql = key_value_model.modifyed(user)

    -- print(sql)
    local bt = moon.clock()

    local res
    for m = 1, 100 do
        res = sqldriver.query(db, sql)
        assert(not res.code, res.message)
    end

    print("test_key_value_table_function insert cost", (moon.clock() - bt)/100)

    sql = string.format([[
        --select userdata
        select key, value from userdata where uid = %d;
    ]], 233)
    local res = sqldriver.query(db, sql)
    local data = res.data

    moon.send("lua", db, "save_then_quit")
end

moon.async(function()
    test_json_query()
    test_big_json_value()
    test_jsonb()

    test_key_value_table_raw()

    test_key_value_table_function()

    test_sql_driver()


    moon.exit(-1)
end)
