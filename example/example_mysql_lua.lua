
local moon = require("moon")

moon.start(function()
    local mysql = require("moon.db.mysql")
    moon.async(function()
            local db, err = mysql.connect({
            host="192.168.81.129",
            port=3306,
            database="game",
            user="root",
            password="4321",
            timeout= 1000,--连接超时ms
            max_packet_size = 1024 * 1024, --数据包最大字节数
        })

        if not db then
            error(err)
        end

        local res = db:query("select * from t_users;")
        print_r(res)
    end)
end)
