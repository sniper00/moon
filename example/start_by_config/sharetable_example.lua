local moon = require("moon")
local sharetable = require("sharetable")
local conf = ... or {}

local file = "sharetable_data.lua"

if conf.agent then
    local data
    local command = {}

    command.LOAD = function()
        data = sharetable.query(file)
        print_r(data)
    end

    command.UPDATE = function()
        data = sharetable.query(file)
        print_r(data)
    end

    moon.dispatch('lua',function(sender, session, cmd, ...)
        local f = command[cmd]
        if f then
            moon.response("lua", sender, session, f(...))
        else
            moon.error(moon.name, "recv unknown cmd "..cmd)
        end
    end)
else
    local content1 = [[
        local M = {
            a = 1,
            b = "hello",
            c = {
                1,2,3,4,5
            }
        }
        return M
    ]]

    local content2 = [[
        local M = {
            a = 2,
            b = "world",
            c = {
                7,8,9,10,11
            }
        }
        return M
    ]]
    local agent = 0
    moon.async(function()
        io.writefile(file, content1)

        moon.new_service("lua",{
            unique = true,
            name = "sharetable",
            file = "../service/sharetable.lua"
        })

        print(sharetable.loadfile(file))

        agent =  moon.new_service(
            "lua",
            {
                name = "agent",
                file = "start_by_config/sharetable_example.lua",
                agent = true
            }
        )

        print(moon.co_call("lua", agent, "LOAD"))
        io.writefile(file, content2)
        print(sharetable.loadfile(file))

        print(moon.co_call("lua", agent, "UPDATE"))

        moon.kill(agent)
        moon.exit(-1)
        -- moon.kill(moon.queryservice("sharetable"))
    end)

end

