local moon = require("moon")
local mysql = require("mysql")

local db = mysql.create()

local ret,errmsg = db:connect("127.0.0.1", 3306, "root", "4321", "mysql",0)
print(ret,errmsg)

local result,errmsg = db:query("select * from article_detail;")
if not result then
    print(errmsg)
    return
end
print_r(result)

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

print(db:execute(create_table))

print(db:execute("INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(1,2,'abc','abc','abc')"))

local stmtid = db:prepare("INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(?,?,?,?,?)")
print(db:execute_stmt(stmtid,2,3,'abc','abc','abc'))