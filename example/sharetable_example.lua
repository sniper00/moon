local moon = require("moon")
local sharetable = require("sharetable")

local data

local file = "sharetable_data.lua"

moon.start(function()
    moon.async(function()
        sharetable.loadfile(file)
        data = sharetable.query(file)
        print(data)
        print_r(data)

        moon.sleep(3000)

        sharetable.loadfile(file)
        sharetable.update(file)
        print(data)
        print_r(data)
    end)
end)