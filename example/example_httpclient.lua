local moon = require("moon")
local httpc = require("moon.http.client")

moon.async(function ()
    --httpc.setproxyhost("127.0.0.1:8081")
    print_r(httpc.get("www.baidu.com"))

    moon.exit(-1)
end)


-- moon.async(function ()
--     print_r(httpc.postform("127.0.0.1:8080","/login",{username="wang",passwd="456",age=110}))
-- end)
