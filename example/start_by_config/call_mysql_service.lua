local moon = require("moon")

local function execute(db, ...)
    moon.send("lua", db, "execute", ...)
end

local function query(db, ...)
    return moon.co_call("lua", db, "query", ...)
end


local db_mysql = moon.queryservice("mysql")

-- local create_table = [[
--     DROP TABLE IF EXISTS `article_detail`;
--     CREATE TABLE `article_detail` (
--     `id` BIGINT NOT NULL,
--     `parentid` BIGINT DEFAULT '0',
--     `title` varchar(255) DEFAULT '',
--     `content` varchar(255) DEFAULT '',
--     `updatetime` varchar(255) DEFAULT '',
--     PRIMARY KEY (`id`)
--     ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
-- ]]

-- execute(db_mysql, create_table)

execute(db_mysql, "INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(1,2,'abc','abc','abc')")

local count = 0
moon.async(function()
    while true do
        local res = query(db_mysql, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = query(db_mysql, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = query(db_mysql, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = query(db_mysql, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.repeated(1000,-1, function()
    print(count)
    count = 0
end)
