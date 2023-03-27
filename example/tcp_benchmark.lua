local moon = require("moon")
local socket = require("moon.socket")
local seri = require("seri")
local binunpack = seri.unpack
local binpack = seri.pack

local conf = ...

conf.host = "127.0.0.1"
conf.port = 33888
conf.count = 1000
conf.client_num = 1000

if conf.name == "server" then
    socket.on("accept",function(fd, msg)
        --print("accept ", fd, moon.decode(msg, "Z"))
    end)
    
    socket.on("message",function(fd, msg)
        socket.write(fd, moon.decode(msg, "Z"))
    end)
    
    socket.on("close",function(fd, msg)
        --print("close ", fd, moon.decode(msg, "Z"))
    end)
    
    local listenfd  = socket.listen(conf.host, conf.port,moon.PTYPE_SOCKET_MOON)
    socket.start(listenfd)
    
    print(string.format([[
        network benchmark run at %s %d with %d clients, per client send %s message.
    ]], conf.host, conf.port, conf.client_num, conf.count))

    return
end

local total,count,client_num,send_count

count = 0

local start_time = 0

local result = {}

local connects = {}

local time_count = {}

local send_data = "Hello World"

local n = 0

local function millseconds()
    return math.floor(moon.clock()*1000)
end

socket.on("connect",function(fd,msg)
    connects[fd] = 1
    n = n + 1
    if n == client_num then
        for k,v in pairs(connects) do
            time_count[k] = millseconds()
            socket.write(k,send_data)
        end
        start_time = millseconds()
        print("running ....")
    end
end)

socket.on("message",function(fd, msg)
    count = count + 1
    local now = millseconds()
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
        local qps = total*1000/(millseconds()-start_time)
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

        moon.kill(moon.queryservice("server"))

        moon.exit(0)
    end
end)

socket.on("close",function(fd, msg)
    --print("close ", fd, moon.decode(msg, "Z"))
end)


total = conf.client_num * conf.count
client_num = conf.client_num
send_count = conf.count

moon.async(function()
    moon.new_service( {
        name = "server",
        file = "tcp_benchmark.lua",
    })

    for _=1,conf.client_num do
        socket.connect(conf.host,conf.port,moon.PTYPE_SOCKET_MOON)
    end
end)









