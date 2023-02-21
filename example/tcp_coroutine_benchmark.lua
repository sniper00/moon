local moon = require("moon")
local socket = require("moon.socket")
local conf = ...

conf.host = conf.host or "127.0.0.1"
conf.port = conf.port or 6770
conf.count = conf.count or 1

local function handle_connection(fd)
    moon.async(function()
        while true do
            local n, err = socket.read(fd,"\r\n")
            if not n then
                print(fd,"closed", err)
                return
            end
            if n == "PING" then
                socket.write(fd, "+PONG\r\n")
            elseif n== "save" then
                socket.write(fd, "*2\r\n$4\r\nsave\r\n$23\r\n3600 1 300 100 60 10000\r\n")
            elseif n== "appendonly" then
                socket.write(fd, "*2\r\n$10\r\nappendonly\r\n$3\r\nyes\r\n")
            end
        end
    end)
end

moon.async(function()
    local listenfd  = socket.listen(conf.host, conf.port, moon.PTYPE_SOCKET_TCP)

    print(string.format([[

        network text benchmark run at %s %d with %d slaves.
        run benchmark use: redis-benchmark -t ping -p %d -c 100 -n 100000
    ]], conf.host, conf.port, conf.count, conf.port))

    while true do
        local fd = socket.accept(listenfd)
        handle_connection(fd)
    end
end)

moon.shutdown(function()
    moon.quit()
end)