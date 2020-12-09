local moon = require("moon")
local socket = require("moon.socket")

local conf = ...

local total,count,client_num,send_count

count = 0

local start_time = 0

local result = {}

local connects = {}

local time_count = {}

local send_data = "Hello World"

local n = 0

socket.on("connect",function(fd,msg)
    connects[fd] = 1
    n = n + 1
    if n == client_num then
        for k,v in pairs(connects) do
            time_count[k] = moon.millsecond()
            socket.write(k,send_data)
        end
        start_time = moon.millsecond()
        print("start....")
    end
end)

socket.on("message",function(fd, msg)
    count = count + 1
    local now = moon.millsecond()
    local diff = now - time_count[fd]
    local v = result[diff]
    if not v then
        v = 0
    end
    result[diff] = v + 1

    local nc = connects[fd]

    if nc < send_count then
        connects[fd] = nc + 1
        time_count[fd] = now
        socket.write(fd,send_data)
        return
    end
    socket.close(fd)
    --print(fd,connects[fd],count,total)

    if count == total then
        print("total ",count)
        local qps = total*1000/(moon.millsecond()-start_time)
        local keys = {}
        for k,_ in pairs(result) do
            table.insert( keys, k)
        end
        table.sort( keys )

        local n = 0
        for _,k in pairs(keys) do
            local v = result[k]
            n = n + v
            print(string.format( "%.02f%% <= %d milliseconds",n/total*100,k))
        end

        print(string.format("%.02f requests per second",qps))
    end
end)

socket.on("close",function(fd, msg)
    --print("close ", fd, moon.decode(msg, "Z"))
end)

socket.on("error",function(fd, msg)
    --print("error ", fd, moon.decode(msg, "Z"))
end)

total = conf.client_num * conf.count
client_num = conf.client_num
send_count = conf.count

moon.async(function()
    moon.sleep(10)
    for _=1,conf.client_num do
        local fd = socket.sync_connect(conf.host,conf.port,moon.PTYPE_SOCKET)
    end
end)





