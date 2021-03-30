local moon = require("moon")
local socket = require("moon.socket")

local conf = ...

local function run_slave()
    local count = 0
    do
        socket.on("accept",function(fd, msg)
            --print("accept ", fd, moon.decode(msg, "Z"))
        end)

        socket.on("message",function(fd, msg)
            --tcpserver.send(fd, moon.decode(msg, "Z"))
            socket.write(fd, moon.decode(msg, "Z"))
            count = count + 1
        end)

        socket.on("close",function(fd, msg)
            --print("close ", fd, moon.decode(msg, "Z"))
        end)

        socket.on("error",function(fd, msg)
            --print("error ", fd, moon.decode(msg, "Z"))
        end)
    end
end


local function run_master()
    local listenfd  = socket.listen(conf.host,conf.port,moon.PTYPE_SOCKET)

    print(string.format([[

        network benchmark run at %s %d with %d slaves.
    ]], conf.host, conf.port, conf.count))

    local slave = {}

    moon.async(function()
        for _=1,conf.count do
            local sid = moon.new_service("lua",{name="slave",file="start_by_config/network_benchmark.lua"})
            table.insert(slave,sid)
        end

        local balance = 1
        while true do
            if balance>#slave then
                balance = 1
            end
            socket.accept(listenfd,slave[balance])
            balance = balance + 1
        end
    end)

    moon.shutdown(function()
        socket.close(listenfd)
    end)
end

if conf.master then
    run_master(conf)
else
    run_slave()
end
