local moon = require("moon")
local conf = ...
local reload = require "hardreload"
local require = reload.require
-- reload.postfix = "_update"	-- for test

local reload_module =  require("reload_module")

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
            print("Hot Fix Result",reload.reload("reload_module","reload_module_update"))
        end)
    end
end)
