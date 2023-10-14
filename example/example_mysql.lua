
local moon = require("moon")

local mysql = require("moon.db.mysql")
moon.async(function()
        local db = mysql.connect({
        host="127.0.0.1",
        port=3306,
        database="mysql",
        user="root",
        password="123456",
        timeout= 1000,--连接超时ms
        max_packet_size = 1024 * 1024, --数据包最大字节数
    })

    if db.code then
        print_r(db)
        return
    end

    local res = db:query("drop table if exists cats")
    res = db:query("create table cats "
                    .."(id serial primary key, ".. "name varchar(5))")
    print_r(res)

    res = db:query("insert into cats (name) "
                            .. "values (\'Bob\'),(\'\'),(null)")
    print_r(res)
    res = db:query("select * from cats order by id asc")
    print_r(res)

    -- multiresultset test
    res = db:query("select * from cats order by id asc ; select * from cats")
    print_r(res, "multiresultset test result=")

    print ("escape string test result=", mysql.quote_sql_str([[\mysql escape %string test'test"]]) )

    -- bad sql statement
    local res =  db:query("select * from notexisttable" )
    print("bad query test result=\n"..print_r(res,true))

    local i=1
    while true do
        local    res = db:query("select * from cats order by id asc")
        print ( "test1 loop times=" ,i,"\n","query result=")
        print_r( res )

        res = db:query("select * from cats order by id asc")
        print ( "test1 loop times=" ,i,"\n","query result=")
        print_r( res )

        moon.sleep(1000)
        i=i+1
    end

    -- db:disconnect()
    -- moon.exit(-1)
end)

