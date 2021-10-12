local kcp = require("moon.kcp")
local moon = require("moon")
local params = ...

if params and params.client then
    for i=1,1000 do
        moon.async(function ()
            moon.sleep(i*10)
            local fd, err = kcp.connect("127.0.0.1", 12347, 1000)
            if fd then
                while true do
                    kcp.send(fd, "hello")
                    local ok = kcp.read(fd, 5)
                    if not ok then
                        print("closed2!!!!")
                        return
                    end
                end
            else
                moon.error(err)
            end
        end)
    end
else
    local counter = 0

    moon.async(function ()
        while true do
            moon.sleep(1000)
            print("count", counter)
            counter = 0
        end
    end)

    local function on_connect(endpoint)
        moon.async(function ()
            -- local bt = moon.clock()
            -- print(kcp.read(endpoint, 5))
            -- print(kcp.read(endpoint, 5))
            -- print("cost", moon.clock() - bt)
            while true do
                local bt = moon.clock()
                local ok = kcp.read(endpoint, 5)
                if not ok then
                    print("closed1!!!!")
                    return
                end
                local cost = moon.clock() - bt
                if cost > 1 then
                	print("cost", moon.clock() - bt)
                end
                kcp.send(endpoint, "world")
                moon.sleep(10)
                counter = counter + 1
            end
        end)
    end

    local function on_close(endpoint)
        print("closed", endpoint)
    end

    kcp.listen("127.0.0.1", 12347, on_connect, on_close)

    moon.async(function ()
        moon.new_service("lua",  {
            name = "receiver",
            file = "example_kcp.lua",
            client = true
        })
    end)
end




