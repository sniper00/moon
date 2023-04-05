------------------------TCP CLIENT----------------------------
local moon = require("moon")
local socket = require("moon.socket")

local HOST = "127.0.0.1"
local PORT = 12345

local MESSAGE_CONTINUED_FLAG = 65535

local function send(fd,data)
    if not fd then
        return false
    end
    local len = #data
    local onesize = 0
    local offset = 0
    repeat
        if len >= MESSAGE_CONTINUED_FLAG then
            onesize = MESSAGE_CONTINUED_FLAG
        else
            onesize = len
        end
        socket.write(fd, string.pack(">H",onesize)..string.sub(data, offset+1, offset + onesize))
        offset = offset + onesize
        len = len - onesize
        --print("write", onesize, "left", len, "offset", offset)
    until len == 0

    if onesize == MESSAGE_CONTINUED_FLAG then
        socket.write(fd, string.pack(">H", 0))
        --print("write", 0)
    end
end

local function session_read( fd )
    if not fd then
        return false
    end

    local message = {}
    repeat
        local data,err = socket.read(fd, 2)
        if not data then
            print(fd,"fd read error",err)
            return false
        end
        local len = string.unpack(">H",data)
        local data2,err2 = socket.read(fd, len)
        if not data2 then
            print(fd,"fd read error",err2)
            return false
        end
        message[#message+1] = data2
        --print("recv", len)
    until len<MESSAGE_CONTINUED_FLAG

    return table.concat(message)
end

moon.async(function()
    local fd,err = socket.connect(HOST, PORT,moon.PTYPE_SOCKET_TCP)
    if not fd then
        print(err)
        return
    end

    print(fd,"Please input('quit' for exit):")
    repeat
        local input = io.read()
        if input then
            if input == "exit" or input == "quit" then
                moon.exit(0)
                return
            end
            send(fd, input)
            local rdata = session_read(fd)
            print("recv ", rdata)
    end
    until(not input)
end)


