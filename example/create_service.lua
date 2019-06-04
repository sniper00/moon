local moon = require("moon")

moon.init(function(conf)
    if conf.slave then
        moon.start(function()
            moon.removeself()
        end)
    else
        moon.repeated(50, 100, function()
            moon.new_service("lua", {
                name = "create_service",
                file = "create_service.lua",
                slave = true
            })
        end)
    end
    return true
end)
