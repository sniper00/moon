local moon = require("moon")
local seri = require("seri")
local sharetable = require("sharetable")

local conf = ...

local file = "sharetable_data.lua"

if conf.agent then
    local data
    local command = {}

    command.LOAD = function()
        data = sharetable.query(file)
        print_r(data)
    end

    command.UPDATE = function()
        sharetable.update(file)
        print_r(data)
    end

    moon.dispatch('lua',function(msg,p)
        local sessionid = msg:sessionid()
        local sender = msg:sender()
        local buf = msg:buffer()
        local cmd = seri.unpack_one(buf)
        local f = command[cmd]
        if f then
            moon.async(function()
                moon.response("lua", sender, sessionid, f(seri.unpack(msg:cstr())))
            end)
        else
            moon.error(moon.name(), "recv unknown cmd "..cmd)
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

    moon.start(function()
        moon.async(function()
            io.writefile(file, content1)
            sharetable.loadfile(file)

            local agent =  moon.new_service(
                "lua",
                {
                    name = "agent",
                    file = "start_by_config/sharetable_example.lua",
                    agent = true
                }
            )

            print(moon.co_call("lua", agent, "LOAD"))
            io.writefile(file, content2)
            sharetable.loadfile(file)

            print(moon.co_call("lua", agent, "UPDATE"))
        end)
    end)
end

