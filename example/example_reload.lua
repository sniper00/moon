local moon = require("moon")
local conf = ...
local reload = require "hardreload"
local require = reload.require
-- reload.postfix = "_update"	-- for test

reload.addsearcher(function(name)
	local file, err = io.open(name..".lua", "rb")
	if file then
        local content = file:read("*a")
        io.close(file)
		return load(content,"@"..name),name
    end
    error(err)
end)

local reload_module = require("reload_before")

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
            print("Hot Fix Result",reload.reload("reload_before", "reload_after"))
        end)
    end
end)
