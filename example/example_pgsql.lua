
local moon = require("moon")
local json = require "json"
local pg = require("moon.db.pg")
moon.async(function()
        local db, err = pg.connect({
        host="127.0.0.1",
        port= 5432,
        database="postgres",
        user="postgres",
        password="123456", -- only support md5 auth
        connect_timeout= 1000,
    })

    if db.code then
        print_r(db)
        return
    end

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


    db:query("drop table if exists game_user")
    print_r(db:query("CREATE TABLE game_user (uid bigint primary key, data text)"))

    print_r(db:query(string.format("INSERT INTO game_user (uid, data) VALUES (1, '%s')", io.readfile("../test/twitter.json"))))

    local sql = [[
update game_user
set
    data = jsonb_set(data, '{user,id}', to_jsonb(15), false)
where uid = 1;
    ]]

    local sql2 =[[
        insert into game_user(uid, data) VALUES(1, '%s') on conflict(uid) do update set data = EXCLUDED.data;
    ]]

    sql2 = string.format(sql2, io.readfile("../test/twitter.json"))

    local res
    local bt = moon.clock()
    for i=1,100 do
        res = db:query(sql2)
        assert(not res.code)
    end
    print("cost", moon.clock() - bt)

    print_r(res)
    -- io.writefile("1.log", json.encode(res))

    db:disconnect()
    moon.exit(-1)
end)
