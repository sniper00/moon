# Moon Lua API 参考文档

本文档详细介绍 Moon 框架的 Lua API。

---

## 核心对象 `moon`

`moon` 是框架的核心对象，提供所有服务管理、消息通信、定时器等功能。

---

## 服务管理

### moon.new_service(params)

创建新服务。

**参数**:
- `params` (table): 服务配置
  - `name` (string): 服务名称
  - `file` (string): 启动脚本路径
  - `unique` (boolean, optional): 是否为唯一服务，默认 false
  - `threadid` (integer, optional): 指定工作线程 ID，默认 0 (自动选择)

**返回**: `integer` - 服务 ID，0 表示创建失败

**示例**:
```lua
local service_id = moon.new_service {
    name = "my_service",
    file = "service/my_service.lua",
    unique = true
}
```

### moon.queryservice(name)

查询唯一服务的 ID。

**参数**:
- `name` (string): 服务名称

**返回**: `integer` - 服务 ID，0 表示不存在

**示例**:
```lua
local service_id = moon.queryservice("my_unique_service")
```

### moon.kill(service_id)

终止指定服务。

**参数**:
- `service_id` (integer): 要终止的服务 ID

**示例**:
```lua
moon.kill(service_id)
```

### moon.quit()

优雅终止当前服务。

关闭所有协程后终止服务。

**示例**:
```lua
moon.quit()
```

### moon.scan_services(workerid)

获取指定工作线程中的所有服务信息。

**参数**:
- `workerid` (integer): 工作线程 ID

**返回**: `string` - JSON 格式的服务信息

**示例**:
```lua
local services_json = moon.scan_services(1)
```

---

## 消息通信

### moon.send(PTYPE, receiver, ...)

发送消息到指定服务。

**参数**:
- `PTYPE` (string): 协议类型，如 "lua", "text"
- `receiver` (integer): 接收者服务 ID
- `...` (any): 消息内容

**示例**:
```lua
moon.send("lua", receiver_id, {cmd = "ping", data = "hello"})
moon.send("text", receiver_id, "plain text message")
```

### moon.call(PTYPE, receiver, ...)

发送请求并等待响应。

**参数**:
- `PTYPE` (string): 协议类型
- `receiver` (integer): 接收者服务 ID
- `...` (any): 请求内容

**返回**: `any...` - 响应内容

**示例**:
```lua
local ok, result = moon.call("lua", db_service, {
    cmd = "query",
    sql = "SELECT * FROM users"
})
```

### moon.response(PTYPE, receiver, sessionid, ...)

响应请求。

**参数**:
- `PTYPE` (string): 协议类型
- `receiver` (integer): 接收者服务 ID
- `sessionid` (integer): 会话 ID
- `...` (any): 响应内容

**示例**:
```lua
moon.dispatch("lua", function(sender, session, data)
    local result = process_request(data)
    moon.response("lua", sender, session, result)
end)
```

### moon.raw_send(PTYPE, receiver, data, session, sender)

发送原始数据（不经过序列化）。

**参数**:
- `PTYPE` (string): 协议类型
- `receiver` (integer): 接收者服务 ID
- `data` (string|buffer): 原始数据
- `session` (integer, optional): 会话 ID
- `sender` (integer, optional): 发送者 ID

**返回**: `integer` - 使用的会话 ID

**示例**:
```lua
moon.raw_send("text", receiver, "raw data", 0, 0)
```

---

## 协议注册

### moon.register_protocol(config)

注册消息协议。

**参数**:
- `config` (table): 协议配置
  - `name` (string): 协议名称
  - `PTYPE` (integer): 协议类型常量
  - `pack` (function): 打包函数
  - `unpack` (function): 解包函数
  - `dispatch` (function): 消息处理函数
  - `israw` (boolean, optional): 是否为原始协议

**示例**:
```lua
moon.register_protocol {
    name = "my_protocol",
    PTYPE = 100,  -- 自定义类型
    pack = function(...) return ... end,
    unpack = function(data) return data end,
    dispatch = function(sender, session, data)
        -- 处理消息
    end
}
```

### moon.dispatch(PTYPE, handler)

设置协议的消息处理函数。

**参数**:
- `PTYPE` (string): 协议类型
- `handler` (function): 处理函数 `(sender, session, ...)`

**示例**:
```lua
moon.dispatch("lua", function(sender, session, data)
    print("Received:", data.cmd)
    if session > 0 then
        moon.response("lua", sender, session, {status = "ok"})
    end
end)
```

### moon.raw_dispatch(PTYPE, handler)

设置原始消息处理函数（接收消息指针）。

**参数**:
- `PTYPE` (string): 协议类型
- `handler` (function): 处理函数 `(message_ptr)`

---

## 协程管理

### moon.async(fn, ...)

异步执行函数。

**参数**:
- `fn` (function): 要执行的函数
- `...` (any): 传递给函数的参数

**返回**: `thread` - 创建的协程

**示例**:
```lua
moon.async(function()
    print("Async task start")
    moon.sleep(1000)
    print("Async task end")
end)
```

### moon.wait(session, receiver, is_raw)

挂起当前协程等待消息或唤醒。

**参数**:
- `session` (integer, optional): 会话 ID
- `receiver` (integer, optional): 接收者服务 ID
- `is_raw` (boolean, optional): 是否返回原始数据

**返回**: `any...` - 消息内容或唤醒参数

**示例**:
```lua
local session = moon.raw_send("lua", receiver, data)
local response = moon.wait(session)
```

### moon.wakeup(co, ...)

手动唤醒挂起的协程。

**参数**:
- `co` (thread): 要唤醒的协程
- `...` (any): 传递给协程的参数

**示例**:
```lua
local co = moon.async(function()
    local result = moon.wait()
    print("Wakeup with:", result)
end)

-- 稍后唤醒
moon.wakeup(co, "hello")
```

### moon.coroutine_num()

获取协程统计信息。

**返回**: `integer, integer` - 运行中的协程数，池中空闲协程数

**示例**:
```lua
local running, free = moon.coroutine_num()
print(string.format("Running: %d, Free: %d", running, free))
```

---

## 定时器

### moon.timeout(mills, fn, profile_trace)

创建一次性定时器。

**参数**:
- `mills` (integer): 延迟毫秒数
- `fn` (function): 回调函数 `(timerid)`
- `profile_trace` (string, optional): 调试追踪标签

**返回**: `integer` - 定时器 ID

**示例**:
```lua
moon.timeout(1000, function()
    print("1 second passed")
end)
```

### moon.sleep(mills, profile_trace)

挂起当前协程指定时间。

**参数**:
- `mills` (integer): 挂起毫秒数
- `profile_trace` (string, optional): 调试追踪标签

**返回**: `boolean, string?` - 正常唤醒返回 true，被 wakeup 返回 false

**示例**:
```lua
moon.async(function()
    print("Start waiting")
    moon.sleep(2000)
    print("2 seconds passed")
end)
```

### moon.remove_timer(timerid)

移除定时器。

**参数**:
- `timerid` (integer): 定时器 ID

**示例**:
```lua
local timerid = moon.timeout(5000, function()
    print("This may not execute")
end)
moon.remove_timer(timerid)
```

---

## 时间函数

### moon.time()

获取当前 UTC 时间戳（秒）。

**返回**: `integer` - Unix 时间戳

**示例**:
```lua
local timestamp = moon.time()
```

### moon.now()

获取服务器启动后的时间（毫秒）。

**返回**: `integer` - 毫秒数

### moon.clock()

获取高精度时间（秒）。

**返回**: `number` - 秒数（带小数）

---

## 序列化

### moon.pack(...)

打包 Lua 对象为二进制格式。

**参数**:
- `...` (any): 要打包的对象

**返回**: `string|buffer` - 打包后的数据

**示例**:
```lua
local data = moon.pack({name = "test", value = 123})
```

### moon.unpack(data, len?)

解包二进制数据为 Lua 对象。

**参数**:
- `data` (string|cstring_ptr): 二进制数据
- `len` (integer, optional): 数据长度

**返回**: `any...` - 解包后的对象

**示例**:
```lua
local obj1, obj2 = moon.unpack(packed_data)
```

---

## 日志

### moon.error(...)

错误级别日志。

### moon.warn(...)

警告级别日志。

### moon.info(...)

信息级别日志。

### moon.debug(...)

调试级别日志。

### moon.DEBUG()

检查是否启用调试日志。

**返回**: `boolean`

**示例**:
```lua
moon.info("Service started")
moon.error("Failed to connect:", err)

if moon.DEBUG() then
    moon.debug("Debug info:", debug_data)
end
```

---

## 环境变量

### moon.env(name, value?)

获取或设置环境变量。

**参数**:
- `name` (string): 变量名
- `value` (string, optional): 变量值（设置时使用）

**返回**: `string` - 变量值

**示例**:
```lua
-- 设置
moon.env("DB_HOST", "localhost")

-- 获取
local host = moon.env("DB_HOST")
```

### moon.env_packed(name, ...)

打包并存储对象到环境。

**参数**:
- `name` (string): 变量名
- `...` (any): 要存储的对象

**返回**: `string` - 打包后的数据

### moon.env_unpacked(name)

从环境获取并解包对象。

**参数**:
- `name` (string): 变量名

**返回**: `any` - 解包后的对象

### moon.args()

获取命令行参数。

**返回**: `string[]` - 参数数组

**示例**:
```lua
-- 启动: ./moon main.lua arg1 arg2
local args = moon.args()
-- args = {"arg1", "arg2"}
```

---

## 服务信息

### moon.id

当前服务 ID。

### moon.name

当前服务名称。

**示例**:
```lua
print(string.format("Service %s (ID: %d)", moon.name, moon.id))
```

---

## 关闭处理

### moon.shutdown(callback)

注册关闭处理函数。

**参数**:
- `callback` (function): 关闭时调用的函数

**示例**:
```lua
moon.shutdown(function()
    print("Service shutting down...")
    save_data()
    moon.quit()
end)
```

### moon.system(cmd, fn)

注册系统命令处理函数。

**参数**:
- `cmd` (string): 命令名
- `fn` (function): 处理函数 `(sender, ...)`

**示例**:
```lua
moon.system("reload", function(sender)
    reload_config()
end)
```

---

## 全局变量管理

### moon.exports

安全地导出全局变量。

**示例**:
```lua
-- 推荐
moon.exports.MY_CONST = 100

-- 不推荐（会触发警告）
-- MY_CONST = 100
```

---

## 协议类型常量

```lua
moon.PTYPE_SYSTEM      = 1   -- 系统消息
moon.PTYPE_TEXT        = 2   -- 文本消息
moon.PTYPE_LUA         = 3   -- Lua 对象
moon.PTYPE_ERROR       = 4   -- 错误消息
moon.PTYPE_DEBUG       = 5   -- 调试消息
moon.PTYPE_SHUTDOWN    = 6   -- 关闭信号
moon.PTYPE_TIMER       = 7   -- 定时器
moon.PTYPE_SOCKET_TCP  = 8   -- TCP
moon.PTYPE_SOCKET_UDP  = 9   -- UDP
moon.PTYPE_SOCKET_WS   = 10  -- WebSocket
moon.PTYPE_SOCKET_MOON = 11  -- MoonSocket
moon.PTYPE_INTEGER     = 12  -- 整数
moon.PTYPE_LOG         = 13  -- 日志
```

---

## 内置协议

框架内置以下协议：

### lua 协议

序列化 Lua 对象进行传输。

```lua
moon.dispatch("lua", function(sender, session, data)
    -- data 是解包后的 Lua 表
end)
```

### text 协议

传输纯文本。

```lua
moon.dispatch("text", function(sender, session, text)
    print("Received text:", text)
end)
```

### integer 协议

传输整数值。

```lua
moon.dispatch("integer", function(sender, session, value)
    print("Received integer:", value)
end)
```

### error 协议

处理错误消息。

```lua
-- 错误协议有默认处理器，打印错误信息
```

---

## 完整示例

```lua
local moon = require("moon")

-- 初始化
print(string.format("Service %s starting...", moon.name))

-- 设置消息处理
moon.dispatch("lua", function(sender, session, data)
    if data.cmd == "ping" then
        if session > 0 then
            moon.response("lua", sender, session, {cmd = "pong"})
        else
            moon.send("lua", sender, {cmd = "pong"})
        end
    elseif data.cmd == "query" then
        local result = query_database(data.sql)
        moon.response("lua", sender, session, result)
    end
end)

-- 定时任务
moon.timeout(1000, function()
    moon.info("Heartbeat")
end)

-- 异步任务
moon.async(function()
    while true do
        moon.sleep(5000)
        cleanup_expired_sessions()
    end
end)

-- 关闭处理
moon.shutdown(function()
    moon.info("Saving data before shutdown...")
    save_all_data()
    moon.quit()
end)

moon.info("Service ready")
```
