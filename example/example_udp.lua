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