# Moon 数据库支持文档

Moon 框架提供了多种数据库的异步驱动支持，包括 Redis、PostgreSQL、MySQL 和 MongoDB。

---

## Redis

**模块**: `moon.db.redis`

Redis 客户端支持完整的 Redis 命令集，包括连接池、Pipeline、Pub/Sub 等功能。

### 连接 Redis

```lua
local redis = require("moon.db.redis")

local db, err = redis.connect({
    host = "127.0.0.1",
    port = 6379,
    auth = "password",     -- 可选
    db = 0,                -- 可选，数据库索引
    timeout = 1000         -- 可选，连接超时（毫秒）
})

if not db then
    moon.error("Redis connect failed:", err)
    return
end
```

### 基本命令

所有 Redis 命令都可以作为方法调用：

```lua
-- 字符串操作
db:set("key", "value")
local value = db:get("key")

-- 设置过期时间
db:setex("key", 3600, "value")

-- 自增
db:incr("counter")
local count = db:get("counter")

-- 哈希操作
db:hset("user:1", "name", "Alice")
db:hset("user:1", "age", 25)
local name = db:hget("user:1", "name")
local user = db:hgetall("user:1")

-- 列表操作
db:lpush("queue", "item1")
db:rpush("queue", "item2")
local item = db:lpop("queue")
local items = db:lrange("queue", 0, -1)

-- 集合操作
db:sadd("set", "member1", "member2")
local members = db:smembers("set")
local is_member = db:sismember("set", "member1")

-- 有序集合
db:zadd("rank", 100, "player1")
db:zadd("rank", 200, "player2")
local top3 = db:zrange("rank", 0, 2, "WITHSCORES")

-- 键操作
db:del("key")
db:exists("key")
db:expire("key", 3600)
local ttl = db:ttl("key")
```

### Pipeline 批量操作

Pipeline 允许一次性发送多个命令，减少网络往返：

```lua
local ops = {
    {"SET", "key1", "value1"},
    {"SET", "key2", "value2"},
    {"GET", "key1"},
    {"INCR", "counter"}
}

-- 执行并获取所有响应
local responses = {}
db:pipeline(ops, responses)
for i, resp in ipairs(responses) do
    print(i, resp.ok, resp.out)
end

-- 只获取最后一个响应
local last_ok, last_result = db:pipeline(ops)
```

### 事务 (MULTI/EXEC)

```lua
db:multi()
db:set("key1", "value1")
db:set("key2", "value2")
local results = db:exec()
```

### Watch 模式 (Pub/Sub)

```lua
local watch = redis.watch({
    host = "127.0.0.1",
    port = 6379
})

-- 订阅频道
watch:subscribe("channel1", "channel2")

-- 模式订阅
watch:psubscribe("news:*")

-- 接收消息（阻塞）
moon.async(function()
    while true do
        local data, channel, pattern = watch:message()
        print("Received:", data, "on", channel)
    end
end)

-- 取消订阅
watch:unsubscribe("channel1")
watch:disconnect()
```

### 错误处理

```lua
local ok, result = db:get("key")
if ok == redis.socket_error then
    moon.error("Socket error:", result)
elseif not ok then
    moon.error("Redis error:", result)
else
    print("Value:", result)
end
```

### 断开连接

```lua
db:disconnect()
```

---

## PostgreSQL

**模块**: `moon.db.pg`

PostgreSQL 客户端实现了完整的 PostgreSQL 协议，支持 SCRAM-SHA-256 认证。

### 连接 PostgreSQL

```lua
local pg = require("moon.db.pg")

local db, err = pg.connect({
    host = "127.0.0.1",
    port = 5432,
    user = "postgres",
    password = "password",
    database = "mydb"
})

if not db then
    moon.error("PostgreSQL connect failed:", err)
    return
end
```

### 执行查询

```lua
-- 简单查询
local ok, result = db:query("SELECT * FROM users WHERE id = 1")
if ok then
    for _, row in ipairs(result) do
        print(row.id, row.name, row.email)
    end
end

-- 参数化查询（推荐，防止 SQL 注入）
local ok, result = db:query(
    "SELECT * FROM users WHERE name = $1 AND age > $2",
    "Alice", 18
)

-- INSERT
local ok, result = db:query(
    "INSERT INTO users (name, email) VALUES ($1, $2) RETURNING id",
    "Bob", "bob@example.com"
)
if ok then
    print("Inserted ID:", result[1].id)
end

-- UPDATE
local ok, result = db:query(
    "UPDATE users SET name = $1 WHERE id = $2",
    "New Name", 1
)

-- DELETE
local ok, result = db:query("DELETE FROM users WHERE id = $1", 1)
```

### 事务

```lua
db:query("BEGIN")
local ok, err = pcall(function()
    db:query("INSERT INTO orders (user_id) VALUES ($1)", 1)
    db:query("UPDATE users SET order_count = order_count + 1 WHERE id = $1", 1)
    db:query("COMMIT")
end)
if not ok then
    db:query("ROLLBACK")
    moon.error("Transaction failed:", err)
end
```

### 断开连接

```lua
db:disconnect()
```

---

## MySQL

**模块**: `moon.db.mysql`

MySQL 客户端提供异步 MySQL 数据库访问。

### 连接 MySQL

```lua
local mysql = require("moon.db.mysql")

local db, err = mysql.connect({
    host = "127.0.0.1",
    port = 3306,
    user = "root",
    password = "password",
    database = "mydb",
    charset = "utf8mb4"
})

if not db then
    moon.error("MySQL connect failed:", err)
    return
end
```

### 执行查询

```lua
-- 查询
local ok, result = db:query("SELECT * FROM users")
if ok then
    for _, row in ipairs(result) do
        print(row.id, row.name)
    end
end

-- 参数化查询
local ok, result = db:query(
    "SELECT * FROM users WHERE name = ? AND status = ?",
    {"Alice", 1}
)

-- INSERT
local ok, result = db:query(
    "INSERT INTO users (name, email) VALUES (?, ?)",
    {"Bob", "bob@example.com"}
)
if ok then
    print("Affected rows:", result.affected_rows)
    print("Insert ID:", result.insert_id)
end
```

---

## MongoDB

**模块**: `moon.db.mongo`

MongoDB 客户端提供 NoSQL 数据库访问。

### 连接 MongoDB

```lua
local mongo = require("moon.db.mongo")

local db, err = mongo.connect({
    host = "127.0.0.1",
    port = 27017,
    database = "mydb"
})
```

### CRUD 操作

```lua
-- 插入
db:insert("users", {name = "Alice", age = 25})

-- 查询
local users = db:find("users", {age = {$gt = 20}})

-- 更新
db:update("users", {name = "Alice"}, {$set = {age = 26}})

-- 删除
db:delete("users", {name = "Alice"})
```

---

## 最佳实践

### 1. 连接管理

在服务初始化时建立连接，服务退出时关闭：

```lua
local moon = require("moon")
local redis = require("moon.db.redis")

local db

-- 初始化
local function init()
    db = redis.connect({
        host = moon.env("REDIS_HOST") or "127.0.0.1",
        port = tonumber(moon.env("REDIS_PORT")) or 6379
    })
    if not db then
        error("Failed to connect to Redis")
    end
end

-- 清理
moon.shutdown(function()
    if db then
        db:disconnect()
    end
    moon.quit()
end)

init()
```

### 2. 错误重试

```lua
local function safe_redis_call(fn, ...)
    local max_retry = 3
    for i = 1, max_retry do
        local ok, result = fn(db, ...)
        if ok == redis.socket_error then
            moon.warn("Redis connection error, reconnecting...")
            db:disconnect()
            db = redis.connect(config)
            if not db then
                moon.sleep(1000)
            end
        else
            return ok, result
        end
    end
    return false, "Redis connection failed after retries"
end
```

### 3. 连接池

对于高并发场景，可以使用连接池：

```lua
local connection_pool = {}
local pool_size = 10

local function get_connection()
    local conn = table.remove(connection_pool)
    if conn then
        return conn
    end
    return redis.connect(config)
end

local function return_connection(conn)
    if #connection_pool < pool_size then
        table.insert(connection_pool, conn)
    else
        conn:disconnect()
    end
end
```

### 4. 使用 Pipeline 优化

批量操作时使用 Pipeline 减少网络往返：

```lua
-- 不推荐：多次往返
for i = 1, 100 do
    db:set("key" .. i, "value" .. i)
end

-- 推荐：Pipeline 批量
local ops = {}
for i = 1, 100 do
    table.insert(ops, {"SET", "key" .. i, "value" .. i})
end
db:pipeline(ops)
```
