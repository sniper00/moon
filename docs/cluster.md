# Moon 集群服务文档

Moon 框架支持多节点集群部署，通过 `cluster` 模块实现跨进程通信。

---

## 概述

集群服务允许不同节点（进程）之间的服务进行通信，就像本地服务调用一样简单。

### 架构图

```
┌─────────────────────┐         ┌─────────────────────┐
│      Node 1         │         │      Node 2         │
│  ┌───────────────┐  │         │  ┌───────────────┐  │
│  │   Service A   │  │  TCP    │  │   Service B   │  │
│  │  (unique)     │◄─┼─────────┼─►│  (unique)     │  │
│  └───────┬───────┘  │  Moon   │  └───────┬───────┘  │
│          │          │  Socket │          │          │
│  ┌───────┴───────┐  │         │  ┌───────┴───────┐  │
│  │    Cluster    │  │         │  │    Cluster    │  │
│  │    Service    │  │         │  │    Service    │  │
│  └───────────────┘  │         │  └───────────────┘  │
└─────────────────────┘         └─────────────────────┘
          │                               │
          └───────────┬───────────────────┘
                      │
              ┌───────┴───────┐
              │  HTTP Config  │
              │    Server     │
              └───────────────┘
```

---

## 配置

### 节点配置服务

集群需要一个 HTTP 服务来提供节点地址信息。每个节点通过 HTTP 请求获取其他节点的地址。

配置 URL 格式：
```
http://config-server/node/{node_id}
```

返回 JSON 格式：
```json
{
    "host": "192.168.1.100",
    "port": 8888
}
```

### 启动集群服务

```lua
-- 启动脚本
local moon = require("moon")

-- 设置当前节点 ID
moon.env("NODE", 1)

-- 启动集群服务
moon.new_service {
    name = "cluster",
    file = "service/cluster.lua",
    unique = true,
    -- 集群配置
    url = "http://config-server:8080/cluster/node/%d"
}
```

---

## API 参考

### cluster.send(node, service, ...)

发送消息到远程服务（不等待响应）。

**参数**:
- `node` (integer): 目标节点 ID
- `service` (string|integer): 目标服务名称（唯一服务）或服务 ID
- `...` (any): 消息内容

**示例**:
```lua
local cluster = require("cluster")

-- 发送到节点 2 的 "gate" 服务
cluster.send(2, "gate", {
    cmd = "broadcast",
    message = "Hello from Node 1"
})
```

### cluster.call(node, service, ...)

发送请求到远程服务并等待响应。

**参数**:
- `node` (integer): 目标节点 ID
- `service` (string|integer): 目标服务名称（唯一服务）或服务 ID
- `...` (any): 请求内容

**返回**: `any...` - 响应内容

**示例**:
```lua
local cluster = require("cluster")

-- 调用节点 2 的 "database" 服务
local ok, result = cluster.call(2, "database", {
    cmd = "query",
    sql = "SELECT * FROM users WHERE id = 1"
})

if ok then
    print("Query result:", result)
else
    print("Error:", result)
end
```

---

## 使用示例

### 1. 定义远程服务

节点 2 - 数据库服务 (`service/database.lua`):

```lua
local moon = require("moon")

moon.dispatch("lua", function(sender, session, data)
    if data.cmd == "query" then
        -- 执行数据库查询
        local result = execute_query(data.sql)
        moon.response("lua", sender, session, true, result)
    elseif data.cmd == "update" then
        -- 执行数据库更新
        local affected = execute_update(data.sql)
        moon.response("lua", sender, session, true, affected)
    end
end)

moon.shutdown(function()
    moon.quit()
end)
```

### 2. 启动服务

节点 1 启动脚本:

```lua
local moon = require("moon")

-- 设置节点 ID
moon.env("NODE", 1)

-- 启动集群服务
moon.new_service {
    name = "cluster",
    file = "service/cluster.lua",
    unique = true,
    url = "http://config-server:8080/cluster/node/%d"
}

-- 启动游戏服务
moon.new_service {
    name = "game",
    file = "service/game.lua",
    unique = true
}
```

节点 2 启动脚本:

```lua
local moon = require("moon")

-- 设置节点 ID
moon.env("NODE", 2)

-- 启动集群服务
moon.new_service {
    name = "cluster",
    file = "service/cluster.lua",
    unique = true,
    url = "http://config-server:8080/cluster/node/%d"
}

-- 启动数据库服务
moon.new_service {
    name = "database",
    file = "service/database.lua",
    unique = true
}
```

### 3. 跨节点调用

在节点 1 的游戏服务中调用节点 2 的数据库服务:

```lua
-- service/game.lua
local moon = require("moon")
local cluster = require("cluster")

moon.dispatch("lua", function(sender, session, data)
    if data.cmd == "get_user" then
        moon.async(function()
            -- 跨节点调用数据库服务
            local ok, user = cluster.call(2, "database", {
                cmd = "query",
                sql = string.format("SELECT * FROM users WHERE id = %d", data.user_id)
            })

            if ok then
                moon.response("lua", sender, session, user)
            else
                moon.response("lua", sender, session, false, "Database error")
            end
        end)
    end
end)
```

---

## 内部机制

### 消息头结构

```lua
---@class cluster_header
---@field to_node integer   -- 接收者节点 ID
---@field to_sname string   -- 接收者服务名称
---@field from_node integer -- 发送者节点 ID
---@field from_addr integer -- 发送者服务 ID
---@field session integer   -- 会话 ID（负数为请求，正数为响应）
```

### 连接管理

- **自动连接**: 首次通信时自动建立连接
- **连接复用**: 同一节点间复用 TCP 连接
- **心跳保活**: 定期发送 ping/pong 消息
- **断线重连**: 连接断开后下次请求自动重连

### 超时处理

- **连接超时**: 1000ms
- **Socket 超时**: 180s
- **心跳间隔**: 5000ms
- **调用超时**: 10s

### 错误处理

当调用失败时，会返回 `false` 和错误信息：

```lua
local ok, err = cluster.call(2, "service", {...})
if not ok then
    moon.error("Cluster call failed:", err)
    -- 可能的错误:
    -- "connect failed: to_node=X err=..."
    -- "socket write failed: to_node=X"
    -- "Cluster call connect closed"
    -- "Cluster call request timeout"
end
```

---

## 性能优化

### 1. 本地调用优化

当目标节点是本地节点时，集群服务会直接使用本地消息传递，避免网络开销：

```lua
-- 如果 receiver_node == NODE，直接本地调用
cluster.send(NODE, "service", {...})  -- 等同于 moon.send
cluster.call(NODE, "service", {...})  -- 等同于 moon.call
```

### 2. 连接池

集群服务自动管理连接池，避免频繁创建连接。

### 3. 批量操作

对于批量跨节点操作，考虑使用 Pipeline 模式：

```lua
moon.async(function()
    local results = {}
    for i, node in ipairs(nodes) do
        results[i] = cluster.call(node, "service", {...})
    end
    -- 处理所有结果
end)
```

---

## 监控与调试

### 集群状态

集群服务会定期输出统计信息：

```
Cluster stats: connections=5, pending_calls=12
```

### 日志级别

可以通过日志查看集群通信详情：

```lua
-- 启用调试日志
moon.env("LOG_LEVEL", "4")  -- 4 = DEBUG
```

---

## 最佳实践

### 1. 服务设计

- 将需要频繁跨节点访问的服务设为唯一服务
- 避免在热点路径上进行跨节点调用
- 考虑数据本地性，减少跨节点数据访问

### 2. 错误处理

```lua
local function safe_cluster_call(node, service, ...)
    local ok, result = cluster.call(node, service, ...)
    if not ok then
        moon.warn(string.format("Cluster call failed: node=%d service=%s err=%s",
            node, service, result))
        return nil, result
    end
    return result
end
```

### 3. 超时控制

```lua
moon.async(function()
    local timerid = moon.timeout(5000, function()
        -- 超时处理
        moon.wakeup(co, false, "timeout")
    end)

    local co = coroutine.running()
    local ok, result = cluster.call(node, service, ...)
    moon.remove_timer(timerid)

    if not ok then
        -- 处理错误
    end
end)
```
