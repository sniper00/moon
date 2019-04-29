local moon = require("moon")

local receiver1
local receiver2
local receiver3
local receiver4

local sttime = 0

local counter = 0

local ncount = 10000

local command = {}

command.TEST = function()
    counter = counter + 1
    if counter == ncount*4 then
        print("cost ",moon.millsecond() - sttime)
        counter = 0
    end
end

local function docmd(header,...)
      local f = command[header]
      if f then
          f(...)
      else
          error(string.format("Unknown command %s", tostring(header)))
      end
end

moon.dispatch('lua',function(msg,p)
    local header = msg:header()
    docmd(header, p.unpack(msg))
end)

moon.start(function()
    receiver1 = moon.queryservice("send_example_receiver1")
    receiver2 = moon.queryservice("send_example_receiver2")
    receiver3 = moon.queryservice("send_example_receiver3")
    receiver4 = moon.queryservice("send_example_receiver4")

    moon.repeated(1000, -1, function()
        sttime = moon.millsecond()
        for _=1,ncount do
            moon.send('lua', receiver1,"TEST","123456789")
            moon.send('lua', receiver2,"TEST","123456789")
            moon.send('lua', receiver3,"TEST","123456789")
            moon.send('lua', receiver4,"TEST","123456789")
        end
    end)
end)



