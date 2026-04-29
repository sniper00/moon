# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Moon is a lightweight game server framework based on the Actor model. Each worker thread can have one or more actors (services) that communicate through message queues. Built with C++17 and Lua, it provides high-performance networking with ASIO and async I/O based on Lua coroutines.

## Build Commands

```shell
# Install premake5 first (required)
brew install premake  # macOS
# or download from https://premake.github.io/download

# Build debug version
premake5 build

# Build release version
premake5 build --release

# Add extension library (e.g., lrust)
preake5 add --package=https://github.com/sniper00/lrust.git

# Remove extension library
premake5 remove --package=https://github.com/sniper00/lrust.git

# Clean build artifacts
premake5 clean

# Run the moon binary
premake5 run example/test/main_test.lua
premake5 run --release example/example_timer.lua
```

## Running Tests

```shell
# Run all tests (requires Redis running on localhost:6379)
./moon example/test/main_test.lua

# Or via premake
premake5 run --release example/test/main_test.lua
```

## Architecture

### Core Components

- **Server** (`src/moon/core/server.h/cpp`): Top-level manager handling worker threads, service lifecycle, signals, and global timers
- **Worker** (`src/moon/core/worker.h/cpp`): Execution thread with independent message queue, supports CPU affinity
- **Service** (`src/moon/core/service.hpp`): Actor abstraction with message dispatch
- **Message** (`src/moon/core/message.hpp`): Inter-service communication unit with source, receiver, session, type, and data

### C++ Core Structure

```
src/moon/
├── main.cpp              # Entry point
├── core/                 # Core framework
│   ├── server.h/cpp      # Server manager
│   ├── worker.h/cpp      # Worker threads
│   ├── service.hpp       # Service base class
│   ├── message.hpp       # Message structure
│   ├── config.hpp        # Constants (PTYPE values)
│   └── network/          # Network layer (ASIO-based)
├── services/
│   └── lua_service.cpp   # Lua service implementation
└── lualib-src/           # C++ Lua bindings
```

### Lua Library Structure

```
lualib/
├── moon.lua              # Core API (moon.send, moon.call, moon.new_service, etc.)
├── moon/
│   ├── api/              # Buffer, serialization APIs
│   ├── db/               # Database drivers (redis, pg, mysql, mongo)
│   ├── http/             # HTTP server/client
│   └── socket.lua        # TCP/UDP socket API
├── seri.lua              # Serialization library
├── json.lua              # JSON library
└── hotfix.lua            # Hot reload support
```

### Message Types (PTYPE)

Defined in `src/moon/core/config.hpp`:
- `PTYPE_SYSTEM` (1): System control
- `PTYPE_TEXT` (2): Plain text
- `PTYPE_LUA` (3): Lua object (serialized)
- `PTYPE_TIMER` (7): Timer events
- `PTYPE_SOCKET_TCP` (8): TCP socket
- `PTYPE_SOCKET_UDP` (9): UDP socket
- `PTYPE_SOCKET_WS` (10): WebSocket
- `PTYPE_SOCKET_MOON` (11): MoonSocket protocol

### Service Communication Patterns

1. **Send** (one-way): `moon.send("lua", receiver_id, data)`
2. **Call/Response** (request-reply): `moon.call("lua", receiver, data)` + `moon.response("lua", sender, session, result)`
3. **Raw Send**: `moon.raw_send("text", receiver, "raw data", session)` - bypasses serialization

### Built-in Services

```
service/
├── cluster.lua           # Cluster service for multi-node deployment
├── sharetable.lua        # Shared table across services
├── redisd.lua            # Redis driver wrapper
└── sqldriver.lua        # SQL database driver wrapper
```

### Extensions (ext/)

```
ext/
├── lrust/                # Rust extension library
```

## Key Lua APIs

```lua
-- Service management
moon.new_service({name="", file="", unique=false, threadid=0})
moon.queryservice(name)  -- Get unique service by name
moon.kill(service_id)
moon.quit()  -- Graceful shutdown

-- Message passing
moon.send(PTYPE, receiver, ...)
moon.call(PTYPE, receiver, ...)  -- Returns results
moon.response(PTYPE, sender, session, ...)
moon.raw_send(PTYPE, receiver, data, session)

-- Coroutine async
moon.async(function() ... end)
moon.sleep(milliseconds)

-- Timer
moon.timeout(milliseconds, function() ... end)

-- Dispatch messages
moon.dispatch("lua", function(sender, session, data) ... end)
```

## Network API

```lua
local socket = require("moon.socket")

-- TCP
socket.listen(ip, port, moon.PTYPE_SOCKET_TCP)
socket.connect(ip, port, moon.PTYPE_SOCKET_TCP)
socket.start(fd)
socket.write(fd, data)
socket.read(fd, delimiter)

-- WebSocket
socket.ws_connect(url)

-- HTTP
local httpd = require("moon.http.server")
httpd.listen(ip, port)
```

## Database API

```lua
-- Redis
local redis = require("moon.db.redis")
local db = redis.connect({host="127.0.0.1", port=6379})
db:get(key)
db:set(key, value)

-- PostgreSQL
local pg = require("moon.db.pg")
local db = pg.connect({host="", port=5432, user="", password="", database=""})
db:query(sql)
```

## Configuration

```lua
-- Environment variables
moon.env("LOG_LEVEL", "4")  -- 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
moon.env("NODE", "1")  -- Node ID for cluster
moon.env("THREAD", "4")  -- Number of worker threads
```

## Documentation

- `docs/architecture.md` - Framework architecture details
- `docs/lua_api_reference.md` - Complete Lua API reference
- `docs/network.md` - Network programming guide
- `docs/database.md` - Database support details
- `docs/cluster.md` - Cluster deployment
- `docs/cpp_service_guide.md` - C++ service development
