local moon = require("moon")

moon.start(function()
    moon.async(function()
        local receiver =  moon.co_new_service("lua", {
            name = "service_coroutine_receiver",
            file = "service_coroutine_receiver.lua"
        })

        print(moon.name(), "call ", receiver, "command", "PING")
        print(moon.co_call("lua", receiver, "PING", "Hello"))
        moon.abort()
    end)
end)



