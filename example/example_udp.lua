local socket = require("moon.socket")

local receiver
receiver = socket.udp(function(str, endpoint)
	print("receiver", str)
	socket.sendto(receiver, endpoint, "world")
end,"127.0.0.1", 12346)

local sender = socket.udp(function(str, endpoint)
	print("sender", str)
end)

print(socket.udp_connect(sender, "127.0.0.1", 12346))

socket.write(sender, "hello")
socket.write(sender, "hello2")
socket.write(sender, "hello3")

---request ntp server
do
local moon = require("moon")

local LI = 0
local VN = 3
local MODE = 3
local STRATUM = 0
local POLL = 4 
local PREC = -6
local JAN_1970 = 0x83aa7e80

local req = {
	[1] = ((LI << 30) | (VN << 27) | (MODE << 24) | (STRATUM << 16) | (POLL << 8) | (PREC & 0xff)),
	[2] = (1 << 16),
	[3] = (1 << 16),
	[4] = 0,
	[5] = 0,
	[6] = 0,
	[7] = 0,
	[8] = 0,
	[9] = 0,
	[10] = 0,
	[11] = moon.time(),
	[12] = (moon.now()%1000)*1000,
}

local data = ""
for _, v in ipairs(req) do
	data = data..string.pack(">I", v)
end

moon.async(function()
	local ntp = socket.udp(function(str, endpoint)
		print("ntp", #str)
		local coarse = string.unpack(">I", str, 4*10+1)
		local fine = string.unpack(">I", str, 4*11+1)
		local sec = coarse - JAN_1970
		local usec = (((fine) >> 12) - 759 * ((((fine) >> 10) + 32768) >> 16))
		print(coarse, fine, sec, usec)
		print(os.date("%c", sec))
	end)
	print(socket.udp_connect(ntp, "time1.cloud.tencent.com", 123))

	socket.write(ntp, data)
end)

end

