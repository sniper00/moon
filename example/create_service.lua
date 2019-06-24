local moon = require("moon")

local conf = ...

moon.start(function()
    if conf.slave then
        moon.quit()
    else
        moon.repeated(50, 100, function()
            moon.new_service("lua", {
                name = "create_service",
                file = "create_service.lua",
                slave = true
            })
        end)
    end
end)


