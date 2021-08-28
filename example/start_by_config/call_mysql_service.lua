local moon = require("moon")
local mysql = require("sqldriver")

local db = moon.queryservice("mysql")

local create_table = [[
    DROP TABLE IF EXISTS `article_detail`;
    CREATE TABLE `article_detail` (
    `id` BIGINT NOT NULL,
    `parentid` BIGINT DEFAULT '0',
    `title` varchar(255) DEFAULT '',
    `content` varchar(255) DEFAULT '',
    `updatetime` varchar(255) DEFAULT '',
    PRIMARY KEY (`id`)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
]]

mysql.execute(db, create_table)

mysql.execute(db, "INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(1,2,'abc','abc','abc')")

local count = 0
moon.async(function()
    while true do
        local res = mysql.query(db, "select * from article_detail;")
        if res then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = mysql.query(db, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = mysql.query(db, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        local res = mysql.query(db, "select * from article_detail;")
        if res.err then
            print_r(res)
        end
        count = count + 1
    end
end)

moon.async(function()
    while true do
        moon.sleep(1000)
        print(count)
        count = 0
    end
end)
