--Base目录里面有一些Lua实现的辅助模块
package.path = 'Base/?.lua'

require("functions")

-- 使用 Base/function.lua 中的class辅助模块
local EchoServer =  class("EchoServer")

function EchoServer:ctor()

end

function EchoServer:Init(config)
    --初始化，可以打印一下config
    print(config)

    -- 获取网络组件
    self.network = _service:get_network()
  
    return true
end

function EchoServer:Start()
	print("EchoServer Start")
end

function EchoServer:Update()

end

function EchoServer:Destory()
	print("EchoServer Destory")
end

function EchoServer:OnNetwork(msg, msgtype)
    local sessionid = msg:sender()
    local data = msg:bytes()

	if msgtype == message_type.network_connect then
        print(string.format("CLIENT CONNECT %d: %s",sessionid,data))
    elseif msgtype == message_type.network_recv then
        --把收到的数据直接返回
        self.network:send(sessionid,data)
        print("recv:"..data,"send "..sessionid)
    elseif msgtype == message_type.network_close then
        print(string.format("CLIENT CLOSE %d: %s",sessionid,data))
    elseif msgtype == message_type.network_error then
        print(string.format("CLIENT %d NETWORK ERROR",sessionid))
    elseif msgtype == message_type.network_logic_error then
        print(string.format("CLIENT %d NETWORK LOGIC ERROR",sessionid))
    end
end

function EchoServer:OnMessage(msg, msgtype)

end

return EchoServer
