local moon = require("moon")
local conf = ...

moon.start(function()
    print("conf:", conf.message)
    print("bye bye")
    -- 使服务退出
    moon.quit()
end)