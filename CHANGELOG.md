# v0.2
- add mongodb driver
- improve message dispatch

 ```lua
    ---old style
    local function docmd(sender,sessionid, CMD,...)

    end

    moon.dispatch('lua',function(msg,unpack)
        local sender, sz, len = moon.decode(msg, "SC")
        docmd(sender, unpack(sz, len))
    end)


    ---new style
    moon.dispatch('lua',function(sender,session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)
 ```

# v0.1 (2021-11-17)
- remove socket.readline. use: socket.read
- bugfix: buffer's move assignment operator
- message object passed by right value in cpp code
- use isocalendar calculate datetime
- improve code