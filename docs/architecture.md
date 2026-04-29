# Moon 框架架构文档

## 概述

Moon 是一个基于 **Actor 模型** 的轻量级游戏服务器框架。每个 Worker 线程可以拥有一个或多个 Actor（Service），它们通过消息队列进行通信。

### 核心特性

- **跨平台**: 支持 Windows、Linux、macOS
- **高性能网络**: 基于 ASIO 的网络调度
- **灵活脚本**: 使用 Lua 进行逻辑开发
- **异步编程**: 基于 Lua 协程的异步模型
- **多协议支持**: TCP、UDP、KCP、WebSocket、HTTP
- **数据库支持**: Redis、PostgreSQL、MongoDB、MySQL 异步驱动

---

## 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         Moon Server                              │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │   Worker 1  │  │   Worker 2  │  │   Worker N │   ...        │
│  │  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │              │
│  │  │Service│  │  │  │Service│  │  │  │Service│  │              │
│  │  ├───────┤  │  │  ├───────┤  │  │  ├───────┤  │              │
│  │  │Service│  │  │  │Service│  │  │  │Service│  │              │
│  │  └───────┘  │  │  └───────┘  │  │  └───────┘  │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
│         │                │                │                      │
│         └────────────────┴────────────────┘                      │
│                          │                                       │
│                  ┌───────┴───────┐                               │
│                  │  Message Queue │                               │
│                  └───────────────┘                               │
├─────────────────────────────────────────────────────────────────┤
│                        Core Components                           │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │ Server  │ │ Worker  │ │ Service │ │ Message │ │  Timer  │   │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                        Network Layer (ASIO)                      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │   TCP   │ │   UDP   │ │   KCP   │ │WebSocket│ │   HTTP  │   │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 核心组件

### 1. Server (服务器)

**文件**: `src/moon/core/server.h/cpp`

服务器是 Moon 框架的顶层管理器，负责：

- 初始化和管理工作线程 (Worker)
- 管理服务的创建和销毁
- 处理信号 (SIGINT, SIGTERM)
- 全局定时器管理
- 环境变量管理

```cpp
// 服务器启动流程
server svr;
svr.init(worker_count, bootstrap_script);
svr.run();
```

### 2. Worker (工作线程)

**文件**: `src/moon/core/worker.h/cpp`

Worker 是执行服务逻辑的线程单元：

- 每个 Worker 有独立的消息队列
- 支持 CPU 亲和性绑定
- 管理服务的生命周期
- 处理定时器事件

### 3. Service (服务)

**文件**: `src/moon/core/service.hpp`

Service 是 Actor 模型的核心抽象：

```cpp
class service {
    // 服务 ID
    uint32_t id_;

    // 服务名称
    std::string name_;

    // 初始化函数
    virtual bool init(const service_conf& conf);

    // 消息分发函数
    virtual void dispatch(message* msg);
};
```

#### 服务类型

| 类型 | 描述 | 实现文件 |
|------|------|----------|
| Lua Service | Lua 脚本实现的服务 | `services/lua_service.cpp` |
| C++ Service | 纯 C++ 实现的服务 | `services/hello_service.cpp` |

#### 服务配置

```cpp
struct service_conf {
    std::string source;    // 脚本路径或源码
    std::string name;      // 服务名称
    uint32_t threadid;     // 工作线程 ID
    bool unique;           // 是否为唯一服务
    // ... 其他配置
};
```

### 4. Message (消息)

**文件**: `src/moon/core/message.hpp`

消息是服务间通信的基本单位：

```cpp
struct message {
    uint32_t source;    // 发送者服务 ID
    uint32_t receiver;  // 接收者服务 ID
    uint32_t session;   // 会话 ID (用于请求-响应模式)
    uint16_t type;      // 消息类型 (PTYPE)
    buffer_ptr data;    // 消息数据
};
```

---

## 消息类型 (PTYPE)

定义在 `src/moon/core/config.hpp`:

| 常量 | 值 | 描述 |
|------|-----|------|
| `PTYPE_SYSTEM` | 1 | 系统控制消息 |
| `PTYPE_TEXT` | 2 | 纯文本消息 |
| `PTYPE_LUA` | 3 | Lua 对象消息 (序列化) |
| `PTYPE_ERROR` | 4 | 错误消息 |
| `PTYPE_DEBUG` | 5 | 调试消息 |
| `PTYPE_SHUTDOWN` | 6 | 关闭信号 |
| `PTYPE_TIMER` | 7 | 定时器事件 |
| `PTYPE_SOCKET_TCP` | 8 | TCP Socket 消息 |
| `PTYPE_SOCKET_UDP` | 9 | UDP Socket 消息 |
| `PTYPE_SOCKET_WS` | 10 | WebSocket 消息 |
| `PTYPE_SOCKET_MOON` | 11 | MoonSocket 消息 |
| `PTYPE_INTEGER` | 12 | 整数消息 |
| `PTYPE_LOG` | 13 | 日志消息 |

---

## 通信模式

### 1. 单向消息 (Send)

```lua
-- 发送消息，不等待响应
moon.send("lua", receiver_service_id, {cmd = "ping"})
```

### 2. 请求-响应 (Call/Response)

```lua
-- 发送请求并等待响应
local result = moon.call("lua", receiver_service_id, {cmd = "query"})

-- 接收方处理并响应
moon.dispatch("lua", function(sender, session, data)
    -- 处理请求
    moon.response("lua", sender, session, {result = "ok"})
end)
```

### 3. 原始消息 (Raw Send)

```lua
-- 发送原始数据，不经过序列化
moon.raw_send("text", receiver, "raw data", session)
```

---

## 目录结构

```
moon/
├── src/moon/                 # C++ 源代码
│   ├── main.cpp              # 程序入口
│   ├── core/                 # 核心组件
│   │   ├── server.h/cpp      # 服务器管理
│   │   ├── worker.h/cpp      # 工作线程
│   │   ├── service.hpp       # 服务基类
│   │   ├── message.hpp       # 消息结构
│   │   └── config.hpp        # 配置常量
│   ├── services/             # 服务实现
│   │   ├── lua_service.cpp   # Lua 服务
│   │   └── hello_service.cpp # 示例 C++ 服务
│   └── network/              # 网络层
│
├── lualib/                   # Lua 库
│   ├── moon.lua              # 核心框架 API
│   ├── moon/db/              # 数据库驱动
│   │   ├── redis.lua         # Redis 客户端
│   │   ├── pg.lua            # PostgreSQL 客户端
│   │   └── mysql.lua         # MySQL 客户端
│   ├── moon/socket.lua       # Socket API
│   ├── moon/http/            # HTTP 模块
│   └── seri.lua              # 序列化库
│
├── service/                  # 内置服务
│   └── cluster.lua           # 集群服务
│
├── example/                  # 示例代码
│   ├── test/                 # 测试用例
│   ├── cluster/              # 集群示例
│   └── *.lua                 # 各种示例
│
├── third/                    # 第三方库
│   ├── lua/                  # Lua 解释器
│   ├── asio/                 # ASIO 网络
│   └── mimalloc/             # 内存分配器
│
└── docs/                     # 文档
```

---

## 设计原则

### 1. Actor 模型

- 每个服务是独立的 Actor
- 状态隔离，无共享内存
- 通过消息传递通信

### 2. 协程异步

- 使用 Lua 协程实现非阻塞 I/O
- 同步风格的异步代码
- 协程池复用提高性能

### 3. 消息驱动

- 统一的消息接口
- 类型化的协议系统
- 灵活的序列化机制

### 4. 模块化

- 服务可独立开发
- 支持热更新
- 松耦合设计

---

## 性能优化

### 1. 内存管理

- 使用 **mimalloc** 高性能内存分配器
- Buffer 对象池复用

### 2. 协程优化

- 协程池避免频繁创建
- 轻量级上下文切换

### 3. 网络优化

- 基于 ASIO 的高性能异步 I/O
- 支持零拷贝数据传输

### 4. 序列化优化

- 高效的 Lua 对象序列化
- 支持 Buffer 直接传递
