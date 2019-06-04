local moon = require("moon")
local socket = require("moon.socket")
local test_assert = require("test_assert")

local HOST = "127.0.0.1"
local PORT = 30003
--------------------------SERVER-------------------------

local listenfd = socket.listen(HOST,PORT,moon.PTYPE_SOCKET)

socket.start(listenfd)

socket.on("accept",function(fd, msg)
    --print("accept ", sessionid, msg:bytes())
end)

socket.on("message",function(fd, msg)
    socket.write_message(fd, msg)
end)

socket.on("close",function(fd, msg)
    --print("close ", sessionid, msg:bytes())
end)

socket.on("error",function(fd, msg)
    --print("error ", sessionid, msg:bytes())
end)

------------------------CLIENT----------------------------

local function send(fd,data)
    if not fd then
        return false
    end
    local len = #data
    return socket.write(fd, string.pack(">H",len)..data)
end

local function session_read( fd )
    if not fd then
        return false
    end
    local data,err = socket.read(fd, 2)
    if not data then
        print(fd,"fd read error",err)
        return false
    end

    local len = string.unpack(">H",data)

    data,err = socket.read(fd, len)
    if not data then
        print(fd,"fd read error",err)
        return false
    end
    return data
end

moon.start(function()
    moon.async(function()
        for i=1,100 do
            local fd,err = socket.connect(HOST,PORT,moon.PTYPE_TEXT)
            if not fd then
                print("connect failed", err)
                return
            end
            moon.async(function ()
                local send_data = tostring(fd)
                send(fd, send_data)
                local rdata = session_read(fd)
                test_assert.equal(rdata, send_data)
                socket.close(fd)
                if i == 100 then
                    socket.close(listenfd)
                    test_assert.success()
                end
            end)
        end
    end)
end)
