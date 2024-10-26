local moon = require("moon")
local json = require("json")
local socket = require("moon.socket")
local test_assert = require("test_assert")
local large_data = {}

for i=1,100000 do
	large_data[i]="HelloWorld"..tostring(i)
end

local data = json.encode(large_data)

socket.on("accept",function(fd, m)
	test_assert.assert(socket.set_enable_chunked(fd,"rw"),"set_enable_chunked failed!")
end)

socket.on("message",function(_, msg)
	test_assert.equal(moon.decode(msg, "Z"),data)
	test_assert.success()
end)

socket.on("close",function(_, msg)
	local m = json.decode(moon.decode(msg, "Z"))
	test_assert.assert(m.code~=800001)
end)

moon.async(function (...)
	local listenfd = socket.listen("127.0.0.1", 30002, moon.PTYPE_SOCKET_MOON)
	print(listenfd)
	socket.start(listenfd)

	local fd = socket.connect("127.0.0.1", 30002, moon.PTYPE_SOCKET_MOON)
	test_assert.assert(fd>0,"connect server failed")
	socket.set_enable_chunked(fd,"rw")
	socket.write(fd, data)
end)





