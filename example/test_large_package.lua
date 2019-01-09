local moon = require("moon")
local json = require("json")
local tcpserver = require("moon.net.tcpserver")
local test_assert = require("test_assert")
local large_data = {}

for i=1,100000 do
	large_data[i]="HelloWorld"..tostring(i)
end

local data = json.encode(large_data)

tcpserver.on("message",function(_, msg)
	test_assert.equal(msg:bytes(),data)
	test_assert.success()
end)

tcpserver.on("error",function(_, msg)
	test_assert.assert(false,msg:bytes())
end)

moon.start(function()
	local connid = tcpserver.connect("127.0.0.1","30002")
	do
		tcpserver.send(connid,data)
	end
end)


