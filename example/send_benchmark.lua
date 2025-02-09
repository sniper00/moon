local moon = require("moon")

local conf = ...

local sttime = 0
local counter = 0


local times = 200

local nreceiver = 8
local ncount = 1000
local avg = 0

if conf.sender then
    local command = {}
    local total_counter = 0
    local total_time = 0
    sttime = moon.clock()
    command.TEST = function()
        counter = counter + 1
        if counter == ncount*nreceiver then
            local cost = moon.clock() - sttime
            avg = avg + math.floor(ncount*nreceiver/cost)
            print(string.format("cost %.03fs, %s op/s", cost, math.floor(ncount*nreceiver/cost)))
            counter = 0
            sttime = moon.clock()
            total_time = total_time + cost
        end
        total_counter = total_counter + 1
    end

    command.RUN = function(receivers)
        local i = 0
        while i < times do    
            for _, id in ipairs(receivers) do
                for _=1,ncount do
                    moon.send('lua', id, "TEST", 1, 2, 3, 4 , {a=1, b=2 , c = {d=1, e=2, f=3}})
                end
            end

            for _, id in ipairs(receivers) do
                assert(moon.call('lua', id, "TEST2"))
            end

            i= i+1
        end

        while total_counter < ncount*nreceiver*times do
            moon.sleep(500)
        end

        for _, id in ipairs(receivers) do
            moon.kill(id)
        end

        print("avg", avg/times, "per sec")
        moon.quit()
        moon.exit(0)
    end

    moon.dispatch('lua',function(sender, session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            f(...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

    moon.shutdown(function ()
        moon.quit()
    end)

    return

elseif conf.receiver then

    local command = {}

    command.TEST = function(sender, ...)
        moon.send('lua', sender, 'TEST', ...)
    end

    command.TEST2 = function(...)
        return true
    end
    
    moon.dispatch('lua',function(sender, session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            moon.response( 'lua', sender, session, f(sender,...))
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)
    return
end


moon.async(function()

    local sender = moon.new_service( {
        unique = true,
        name = "sender",
        file = "send_benchmark.lua",
        sender = true
    })
    assert(sender>0)

    local receivers = {}
    for i=1, nreceiver do
        local id = moon.new_service( {
            name = "receiver",
            file = "send_benchmark.lua",
            receiver = true
        })

        table.insert(receivers, id)
    end

    moon.send('lua', moon.queryservice('sender'), "RUN", receivers)
end)





