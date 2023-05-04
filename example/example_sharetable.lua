local moon = require("moon")
local sharetable = require("sharetable")
local conf = ...

local name = "sharedata"

if conf.agent then
    local data
    local command = {}

    command.LOAD = function()
        local res = sharetable.queryall()
        for k, v in pairs(res) do
            print(k, v)
        end
        data = res[name]
        print_r(data)
        return true
    end

    command.UPDATE = function()
        data = sharetable.query(name..".lua")
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
    local content_old = [[
        local M = {
            a = 1,
            b = "hello",
            c = {
                1,2,3,4,5
            }
        }
        return M
    ]]

    local content_new = [[
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

        io.writefile("./table/"..name..".lua", content_old)

        moon.new_service({
            unique = true,
            name = "sharetable",
            file = "../service/sharetable.lua",
            dir = "./table"
        })

        agent =  moon.new_service(
            {
                name = "agent",
                file = "example_sharetable.lua",
                agent = true
            }
        )

        require("fs").mkdir("./table")

        print(moon.call("lua", agent, "LOAD"))

        io.writefile("./table/"..name..".lua", content_new)
        print(sharetable.loadfile(name..".lua"))
        require("fs").remove("./table/"..name..".lua")

        print(moon.call("lua", agent, "UPDATE"))

        moon.kill(agent)
        moon.sleep(1000)
        moon.kill(moon.queryservice("sharetable"))
        moon.exit(0)
    end)
end