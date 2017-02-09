local string = require("string")
local socket = require("socket")

--moonnet 框架默认的通信协议是 2字节数据长度（小端）+ 数据，例如发送 "ABCD" ,那么发送的数据格式应该是 4(uint16)ABCD。

local function pack_msg(data)
	local len = #data
	return string.pack("<Hc"..len,len,data)
end

local function do_recv(sock)
	--获取消息长度
	local response, receive_status = sock:receive(2)
	if receive_status == "closed" then
		return false
	end

    if response then
        local len = string.unpack("<H", response)
            --获取消息数据
        response, receive_status = sock:receive(len)
        print(response)        
    end
    return true 
end

host = "127.0.0.1"
port = 11111

c = socket.connect(host,port)

assert(c~=nil)

c:settimeout(0)


local data = io.read()

local recvt,sendt,status
while data ~= 'exit' do
	local smsg = pack_msg(data)
	assert(c:send(smsg))

	recvt, sendt, status = socket.select({c}, nil, 1)
	while #recvt > 0 do
		if not do_recv(c) then
			break
		end
        recvt, sendt, status = socket.select({c}, nil, 1)
	end

    data = io.read()
end

c:close()


