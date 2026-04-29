# Moon 网络编程文档

Moon 框架提供高性能的网络通信支持，包括 TCP、UDP、KCP、WebSocket 和 HTTP 协议。

---

## 概述

Moon 的网络层基于 ASIO 库实现，提供异步非阻塞的 I/O 操作。

### 支持的协议

| 协议 | 消息类型 | 描述 |
|------|----------|------|
| TCP | `PTYPE_SOCKET_TCP` | 可靠的面向连接协议 |
| UDP | `PTYPE_SOCKET_UDP` | 无连接数据报协议 |
| KCP | - | 可靠 UDP 协议（游戏优化） |
| WebSocket | `PTYPE_SOCKET_WS` | 全双工 Web 通信 |
| MoonSocket | `PTYPE_SOCKET_MOON` | Moon 自定义协议 |

---

## TCP 编程

### 服务器端

```lua
local moon = require("moon")
local socket = require("moon.socket")

-- 创建监听
local listenfd = socket.listen("0.0.0.0", 8888, moon.PTYPE_SOCKET_TCP)
socket.start(listenfd)

print("Server listening on port 8888")
```

### 处理连接事件

MoonSocket 协议使用事件回调处理：

```lua
local moon = require("moon")
local socket = require("moon.socket")

-- 创建监听 (使用 MoonSocket 协议)
local listenfd = socket.listen("0.0.0.0", 8888, moon.PTYPE_SOCKET_MOON)
socket.start(listenfd)

-- 连接事件
socket.on("connect", function(fd, msg)
    local addr = moon.decode(msg, "Z")
    print("Connected:", fd, addr)
end)

-- 接受连接事件
socket.on("accept", function(fd, msg)
    local addr = moon.decode(msg, "Z")
    print("Accepted:", fd, addr)
    socket.settimeout(fd, 60)  -- 设置超时
    socket.set_enable_chunked(fd, "wr")  -- 启用分块传输
end)

-- 消息事件
socket.on("message", function(fd, msg)
    local data = moon.decode(msg, "B")  -- 获取 buffer
    print("Message from:", fd)

    -- 回显
    socket.write(fd, data)
end)

-- 关闭事件
socket.on("close", function(fd, msg)
    local reason = moon.decode(msg, "Z")
    print("Closed:", fd, reason)
end)
```

### 客户端

```lua
local moon = require("moon")
local socket = require("moon.socket")

moon.async(function()
    -- 连接服务器
    local fd, err = socket.connect("127.0.0.1", 8888, "tcp")
    if not fd then
        print("Connect failed:", err)
        return
    end

    -- 设置无延迟
    socket.setnodelay(fd)

    -- 发送数据
    socket.write(fd, "Hello, Server!")

    -- 读取数据（指定分隔符）
    local data, err = socket.read(fd, "\n")
    if not data then
        print("Read failed:", err)
        socket.close(fd)
        return
    end
    print("Server response:", data)

    -- 读取固定长度
    local header = socket.read(fd, 4)  -- 读取 4 字节

    socket.close(fd)
end)
```

---

## Socket API 参考

### socket.listen(host, port, protocol)

创建 TCP 监听 socket。

**参数**:
- `host` (string): 绑定地址
- `port` (integer): 端口号
- `protocol` (integer): 协议类型

**返回**: `integer` - 监听 socket 的 fd

### socket.start(listenfd)

开始接受连接。

**参数**:
- `listenfd` (integer): 监听 socket 的 fd

### socket.accept(listenfd, serviceid?)

异步接受新连接。

**参数**:
- `listenfd` (integer): 监听 socket 的 fd
- `serviceid` (integer, optional): 处理连接的服务 ID

**返回**: `integer|nil, string?` - 新连接的 fd 或错误信息

### socket.connect(host, port, protocol, timeout?)

异步连接远程服务器。

**参数**:
- `host` (string): 目标地址
- `port` (integer): 目标端口
- `protocol` (string|integer): 协议 ("tcp" 或 "moon")
- `timeout` (integer, optional): 超时时间（毫秒）

**返回**: `integer|nil, string?` - 连接的 fd 或错误信息

### socket.read(fd, delim, maxcount?)

从 socket 读取数据。

**参数**:
- `fd` (integer): socket fd
- `delim` (string|integer): 分隔符字符串或读取字节数
- `maxcount` (integer, optional): 最大读取字节数

**返回**: `string|nil, string?` - 读取的数据或错误信息

```lua
-- 读取一行（以 \n 结尾）
local line = socket.read(fd, "\n")

-- 读取直到 \r\n\r\n（HTTP 头结束）
local header = socket.read(fd, "\r\n\r\n")

-- 读取固定字节数
local data = socket.read(fd, 1024)
```

### socket.write(fd, data)

向 socket 写入数据。

**参数**:
- `fd` (integer): socket fd
- `data` (string|buffer): 要写入的数据

**返回**: `boolean` - 是否成功

### socket.write_raw(fd, data)

写入原始数据（绕过编码）。

### socket.write_then_close(fd, data)

写入数据后关闭连接。

### socket.close(fd)

关闭 socket。

**参数**:
- `fd` (integer): socket fd

### socket.setnodelay(fd)

设置 TCP_NODELAY 选项（禁用 Nagle 算法）。

### socket.settimeout(fd, seconds)

设置 socket 超时。

**参数**:
- `fd` (integer): socket fd
- `seconds` (integer): 超时秒数

### socket.set_enable_chunked(fd, mode)

启用分块传输模式。

**参数**:
- `fd` (integer): socket fd
- `mode` (string): "r"（读）、"w"（写）或 "wr"（读写）

### socket.on(event, callback)

注册事件回调（用于 MoonSocket 协议）。

**参数**:
- `event` (string): 事件类型 ("connect", "accept", "message", "close")
- `callback` (function): 回调函数 `function(fd, msg)`

---

## UDP 编程

### UDP 服务器

```lua
local moon = require("moon")
local socket = require("moon.socket")

-- 创建 UDP socket
local fd = socket.udp(function(data, from)
    print("Received from:", from)
    print("Data:", data)

    -- 回送数据 (from 格式为 "ip:port")
    socket.sendto(fd, from, "Echo: " .. data)
end, "0.0.0.0", 9999)

print("UDP server listening on port 9999")
```

### UDP 客户端

```lua
local moon = require("moon")
local socket = require("moon.socket")

-- 创建 UDP socket
local fd = socket.udp(function(data, from)
    print("Response:", data)
end)

-- 发送数据
socket.sendto(fd, "127.0.0.1:9999", "Hello UDP!")

-- 关闭
socket.close(fd)
```

---

## WebSocket

### WebSocket 服务器

```lua
local moon = require("moon")
local websocket = require("moon.socket.websocket")

-- 创建 WebSocket 服务器
local server = websocket.listen("0.0.0.0", 8080, {
    on_open = function(fd, path)
        print("WebSocket opened:", fd, path)
    end,
    on_message = function(fd, data, opcode)
        print("Message:", data, opcode)
        server:send(fd, "Echo: " .. data)
    end,
    on_close = function(fd, code, reason)
        print("WebSocket closed:", fd, code, reason)
    end
})
```

### WebSocket 客户端

```lua
local moon = require("moon")
local websocket = require("moon.socket.websocket")

moon.async(function()
    local ws, err = websocket.connect("ws://127.0.0.1:8080/ws")
    if not ws then
        print("Connect failed:", err)
        return
    end

    ws:send("Hello WebSocket!")

    while true do
        local data, opcode = ws:recv()
        if not data then
            print("Connection closed")
            break
        end
        print("Received:", data)
    end

    ws:close()
end)
```

---

## HTTP

### HTTP 服务器

```lua
local moon = require("moon")
local httpd = require("moon.http.server")

local server = httpd.listen("0.0.0.0", 8080)

-- 路由处理
server:on("/", function(req, res)
    res:write("Welcome to Moon HTTP Server")
end)

server:on("/api/json", function(req, res)
    res:header("Content-Type", "application/json")
    res:write_json({
        status = "ok",
        time = moon.time()
    })
end)

server:on("/api/post", function(req, res)
    local body = req:json()
    res:write_json({received = body})
end)

print("HTTP server on port 8080")
```

### HTTP 客户端

```lua
local moon = require("moon")
local httpc = require("moon.http.client")

moon.async(function()
    -- GET 请求
    local resp = httpc.get("http://httpbin.org/get")
    print(resp.status_code)
    print(resp.body)

    -- POST 请求
    local resp = httpc.post("http://httpbin.org/post", {
        body = '{"key":"value"}',
        headers = {
            ["Content-Type"] = "application/json"
        }
    })
    print(resp.body)
end)
```

---

## KCP (可靠 UDP)

KCP 是一个快速可靠的 ARQ 协议，适合游戏实时通信。

```lua
local moon = require("moon")
local kcp = require("moon.kcp")

-- 创建 KCP 会话
local session = kcp.create(conv_id, function(data)
    -- 发送 UDP 数据
    socket.sendto(udp_fd, remote_addr, data)
end)

-- 设置参数
kcp.nodelay(session, 1, 10, 2, 1)
kcp.wndsize(session, 128, 128)

-- 发送数据
kcp.send(session, "Hello KCP")

-- 接收数据 (在 UDP 回调中)
local function on_udp_data(data)
    kcp.input(session, data)
    local msg = kcp.recv(session)
    if msg then
        print("KCP received:", msg)
    end
end

-- 定时更新
moon.async(function()
    while true do
        moon.sleep(10)
        kcp.update(session)
    end
end)
```

---

## 消息解码

Moon 提供多种消息解码方式：

```lua
-- 解码为字符串
local str = moon.decode(msg, "Z")

-- 解码为 buffer
local buf = moon.decode(msg, "B")

-- 解码为长度前缀数据
local data = moon.decode(msg, "L")

-- 解码 socket 事件信息
local fd, sdt = moon.decode(msg, "SR")

-- 解码 sender, session, buffer
local sender, session, buf = moon.decode(msg, "SEB")
```

---

## 最佳实践

### 1. 连接管理

```lua
local connections = {}

socket.on("accept", function(fd, msg)
    connections[fd] = {
        fd = fd,
        addr = moon.decode(msg, "Z"),
        last_active = moon.time()
    }
end)

socket.on("close", function(fd, msg)
    connections[fd] = nil
end)
```

### 2. 心跳检测

```lua
moon.async(function()
    while true do
        moon.sleep(30000)  -- 30 秒
        local now = moon.time()
        for fd, conn in pairs(connections) do
            if now - conn.last_active > 60 then
                socket.close(fd)
            else
                socket.write(fd, pack_heartbeat())
            end
        end
    end
end)
```

### 3. 消息封包

```lua
local buffer = require("buffer")

-- 封包：长度 + 数据
local function pack_message(data)
    return buffer.pack("IS", #data, data)
end

-- 解包
local function unpack_message(msg)
    local len, data = moon.decode(msg, "IS")
    return data
end
```

### 4. 错误处理

```lua
socket.on("message", function(fd, msg)
    local ok, err = pcall(function()
        process_message(fd, msg)
    end)
    if not ok then
        moon.error("Process message error:", err)
        socket.close(fd)
    end
end)
```

---

## 性能优化

### 1. 使用 Buffer

避免频繁的字符串拼接，使用 buffer：

```lua
local buffer = require("buffer")

local buf = buffer.new()
buf:write_u32(len)
buf:write_string(data)
socket.write(fd, buf:to_string())
```

### 2. 批量发送

合并小消息减少系统调用：

```lua
local message_queue = {}

local function flush_messages(fd)
    if #message_queue > 0 then
        socket.write(fd, buffer.concat(message_queue))
        message_queue = {}
    end
end
```

### 3. 连接池

复用连接减少连接开销：

```lua
local connection_pool = {}

local function get_connection(host, port)
    local key = host .. ":" .. port
    if connection_pool[key] then
        return table.remove(connection_pool[key])
    end
    return socket.connect(host, port, "tcp")
end
```
