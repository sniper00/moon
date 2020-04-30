local moon = require("moon")
local conf = ...
local reload = require "reload"
-- reload.postfix = "_update"	-- for test

local context = {}
context.tb = {}

local reload_module =  loadfile("reload_module")
reload_module = reload_module(context)
package.loaded["reload_module"] = reload_module


moon.start(function ()
    moon.async(function ()
        local rmd = reload_module.new()
        while true do
            rmd:func()
        end
    end)

    if not conf.new then
        print("10 seconds later hot fix")
        moon.repeated(10000,1,function (  )
            print("Hot Fix Result",reload.reload { "reload_module" })
            -- moon.async(function ()
            --     moon.co_new_service("lua",{file="reload_example.lua",name="re",new = true})
            -- end)
        end)
    end
end)
