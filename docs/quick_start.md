# Moon 快速入门指南

本指南帮助您快速上手 Moon 框架。

---

## 环境要求

- **操作系统**: Windows / Linux / macOS
- **编译器**: VS2022 17.5+ / GCC 9.3+ / Clang 9.0+ (支持 C++17)
- **构建工具**: [Premake5](https://premake.github.io/download)

---

## 安装

### 1. 下载预编译版本

从 [Releases](https://github.com/sniper00/moon/releases/tag/prebuilt) 下载预编译的二进制文件。

### 2. 从源码编译

```bash
# 克隆仓库
git clone https://github.com/sniper00/moon.git
cd moon

# 编译 (Debug 版本)
premake5 build

# 编译 (Release 版本)
premake5 build --release
```

---

## 第一个服务

创建文件 `hello.lua`:

```lua
local moon = require("moon")

print("Hello, Moon!")
print(string.format("Service: %s (ID: %d)", moon.name, moon.id))

-- 设置消息处理
moon.dispatch("lua", function(sender, session, data)
    print("Received:", data.message)

    -- 如果是请求，发送响应
    if session > 0 then
        moon.response("lua", sender, session, {reply = "OK"})
    end
end)

-- 定时器示例
moon.timeout(1000, function()
    print("1 second timer fired!")
end)

-- 异步任务示例
moon.async(function()
    print("Async task start")
    moon.sleep(2000)
    print("Async task end (after 2 seconds)")
end)

print("Service initialized")
```

运行:
```bash
./moon hello.lua
```

---

## 服务间通信

创建两个服务来演示通信。

### 服务 A (`service_a.lua`)

```lua
local moon = require("moon")

moon.dispatch("lua", function(sender, session, data)
    if data.cmd == "ping" then
        print("Service A received ping")
        moon.response("lua", sender, session, {cmd = "pong"})
    end
end)

-- 启动服务 B
local service_b = moon.new_service {
    name = "service_b",
    file = "service_b.lua"
}

-- 发送消息给服务 B
moon.send("lua", service_b, {cmd = "hello", from = "A"})

-- 请求-响应模式
moon.async(function()
    local ok, result = moon.call("lua", service_b, {cmd = "ping"})
    print("Service B response:", result.cmd)
end)

print("Service A started")
```

### 服务 B (`service_b.lua`)

```lua
local moon = require("moon")

moon.dispatch("lua", function(sender, session, data)
    print("Service B received:", data.cmd)

    if data.cmd == "ping" then
        moon.response("lua", sender, session, {cmd = "pong"})
    end
end)

print("Service B started")
```

运行:
```bash
./moon service_a.lua
```

---

## 唯一服务

唯一服务可以通过名称查找，适合作为全局服务。

```lua
-- 创建唯一服务
moon.new_service {
    name = "database",
    file = "service/database.lua",
    unique = true  -- 标记为唯一服务
}

-- 在其他服务中查找
local db_service = moon.queryservice("database")
if db_service > 0 then
    local result = moon.call("lua", db_service, {cmd = "query"})
end
```

---

## 定时器

```lua
local moon = require("moon")

-- 一次性定时器
moon.timeout(1000, function()
    print("One-time timer")
end)

-- 周期性定时器（通过递归实现）
local function periodic()
    print("Periodic task")
    moon.timeout(1000, periodic)
end
periodic()

-- 使用 sleep 实现周期任务
moon.async(function()
    while true do
        moon.sleep(5000)
        print("Every 5 seconds")
    end
end)
```

---

## 网络服务

### TCP 服务器

```lua
local moon = require("moon")
local socket = require("moon.socket")

-- 监听端口
local listenfd = socket.listen("0.0.0.0", 8888, moon.PTYPE_SOCKET_TCP)
socket.start(listenfd)

print("TCP server listening on port 8888")

-- 处理连接事件
socket.on("accept", function(fd, msg)
    print("New client connected:", fd)
    socket.settimeout(fd, 60)
end)

socket.on("message", function(fd, msg)
    local data = moon.decode(msg, "Z")  -- 解码为字符串
    print("Received:", data)

    -- 回显
    socket.write(fd, "Echo: " .. data)
end)

socket.on("close", function(fd, msg)
    print("Client disconnected:", fd)
end)
```

### TCP 客户端

```lua
local moon = require("moon")
local socket = require("moon.socket")

moon.async(function()
    -- 连接服务器
    local fd, err = socket.connect("127.0.0.1", 8888, moon.PTYPE_SOCKET_TCP)
    if not fd then
        print("Connect failed:", err)
        return
    end

    -- 发送数据
    socket.write(fd, "Hello, Server!")

    -- 接收数据
    while true do
        local data, err = socket.read(fd, "\n")
        if not data then
            print("Connection closed:", err)
            break
        end
        print("Server response:", data)
    end
end)
```

---

## HTTP 服务

```lua
local moon = require("moon")
local httpd = require("moon.http.server")

-- 创建 HTTP 服务器
local server = httpd.listen("0.0.0.0", 8080)

-- 处理请求
server:on("/api/hello", function(request, response)
    response:write_json({message = "Hello, World!"})
end)

server:on("/api/user", function(request, response)
    if request.method == "GET" then
        response:write_json({id = 1, name = "Alice"})
    elseif request.method == "POST" then
        local data = request:json()
        response:write_json({status = "created", user = data})
    end
end)

print("HTTP server listening on port 8080")
```

---

## 数据库访问

### Redis

```lua
local moon = require("moon")
local redis = require("moon.db.redis")

moon.async(function()
    local db = redis.connect {
        host = "127.0.0.1",
        port = 6379
    }

    -- 设置值
    db:set("mykey", "myvalue")

    -- 获取值
    local value = db:get("mykey")
    print("Value:", value)

    db:disconnect()
end)
```

### PostgreSQL

```lua
local moon = require("moon")
local pg = require("moon.db.pg")

moon.async(function()
    local db = pg.connect {
        host = "127.0.0.1",
        port = 5432,
        user = "postgres",
        password = "password",
        database = "mydb"
    }

    -- 查询
    local ok, result = db:query("SELECT * FROM users")
    if ok then
        for _, row in ipairs(result) do
            print(row.id, row.name)
        end
    end

    db:disconnect()
end)
```

---

## 热更新

Moon 支持运行时更新 Lua 代码而不重启服务。

```lua
-- 重新加载模块
local hotfix = require("hotfix")
hotfix.reload("my_module")

-- 或使用代码缓存清除
require("codecache").clear()
```

详细热更新文档请参考 `lualib/hotfix_usage_zh.md`。

---

## 调试

### 日志级别

```lua
-- 设置日志级别
moon.env("LOG_LEVEL", "4")  -- 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG

-- 使用日志
moon.error("Error message")
moon.warn("Warning message")
moon.info("Info message")
moon.debug("Debug message")
```

### 远程调试

```lua
-- 发送调试命令
local debug_service = moon.queryservice("debug")
if debug_service > 0 then
    -- 获取内存使用
    local mem = moon.call("debug", debug_service, "mem")
    print("Memory usage:", mem, "KB")

    -- 触发 GC
    moon.call("debug", debug_service, "gc")

    -- 获取状态
    local state = moon.call("debug", debug_service, "state")
    print(state)
end
```

---

## 项目结构建议

```
my_project/
├── main.lua           # 主入口
├── config.lua         # 配置
├── service/           # 服务目录
│   ├── gate.lua       # 网关服务
│   ├── game.lua       # 游戏逻辑
│   └── db.lua         # 数据库服务
├── lib/               # 公共库
│   ├── protocol.lua   # 协议定义
│   └── utils.lua      # 工具函数
└── proto/             # 协议文件
    └── game.proto
```

---

## 命令行参数

```bash
# 运行脚本
./moon main.lua

# 带参数运行
./moon main.lua arg1 arg2 arg3

# 在脚本中获取参数
local args = moon.args()
-- args = {"arg1", "arg2", "arg3"}
```

---

## 下一步

- 阅读 [架构文档](architecture.md) 了解框架设计
- 查看 [Lua API 参考](lua_api_reference.md) 了解详细 API
- 学习 [数据库支持](database.md) 和 [集群服务](cluster.md)
- 查看 `example/` 目录中的示例代码

---

## 常见问题

### Q: 如何处理大量并发连接？

A: Moon 使用 ASIO 异步 I/O，单个节点可以处理数万并发连接。建议根据业务逻辑复杂度调整工作线程数量。

### Q: 服务之间如何共享数据？

A: 推荐使用消息传递。如需共享只读数据，可以使用 `sharetable` 模块或 Redis。

### Q: 如何实现服务的负载均衡？

A: 可以创建多个非唯一服务，由网关服务进行调度分发。

### Q: 如何监控服务状态？

A: 使用 `moon.scan_services()` 获取服务列表，或实现自定义的监控服务定期收集状态。
