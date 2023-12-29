local moon = require("moon")

local conf = ...

if conf.recever then
    local command = {}

    command.TEST = function(sender, ...)
        moon.send('lua', sender, 'TEST', ...)
    end

    moon.dispatch('lua',function(sender, session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)

    moon.shutdown(function ()
        moon.quit()
    end)

    return true
end


local sttime = 0
local counter = 0


local times = 100

local nreceiver = 32
local ncount = 100
local avg = 0

local command = {}

command.TEST = function()
    counter = counter + 1
    if counter == ncount*nreceiver then
        local cost = moon.clock() - sttime
        avg = avg + math.floor(ncount*nreceiver/cost)
        print(string.format("cost %.03fs, %s op/s", cost, math.floor(ncount*nreceiver/cost)))
        counter = 0
    end
end

moon.dispatch('lua',function(sender, session, cmd, ...)
    local f = command[cmd]
    if f then
        f(...)
    else
        error(string.format("Unknown command %s", tostring(cmd)))
    end
end)



moon.async(function()

    local receivers = {}

    for i=1, nreceiver do
        local id = moon.new_service( {
            unique = true,
            name = "receiver"..i,
            file = "send_benchmark.lua",
            recever = true
        })

        table.insert(receivers, id)
    end

    local i = 0
    while i < times do
        sttime = moon.clock()

        for _=1,ncount do
            for _, id in ipairs(receivers) do
                moon.send('lua', id, "TEST", 1, 2, 3, 4 , {a=1, b=2 , c = {d=1, e=2, f=3}})
            end
        end

        moon.sleep(0)
        i= i+1
    end

    moon.sleep(500)

    for _, id in ipairs(receivers) do
        moon.kill(id)
    end
    moon.quit()

    print("avg", avg/times)
end)





