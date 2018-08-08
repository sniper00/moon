local mysql = require("mysql")

local conn = mysql.create()

local ret,errmsg = conn:connect("192.168.182.123", 3306, "moon", "12345", "mytest",2)
print(ret,errmsg)

local result = conn:query("select * from article_detail;")
log.dump(result)

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

print(conn:execute(create_table))

print(conn:execute("INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(1,2,'abc','abc','abc')"))

local stmtid = conn:prepare("INSERT INTO article_detail(id,parentid,title,content,updatetime) VALUES(?,?,?,?,?)")
print(conn:execute_stmt(stmtid,2,3,'abc','abc','abc'))