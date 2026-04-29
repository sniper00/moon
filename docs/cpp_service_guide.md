# Moon C++ Service Development Guide - Hello Service Example

This document provides a detailed guide on how to create a pure C++ Service in the Moon framework, using the `hello` service as an example.

## Table of Contents

- [Overview](#overview)
- [File Structure](#file-structure)
- [Step 1: Create Header File](#step-1-create-header-file)
- [Step 2: Implement the Service](#step-2-implement-the-service)
- [Step 3: Register the Service](#step-3-register-the-service)
- [Step 4: Use the Service](#step-4-use-the-service)
- [Core Concepts](#core-concepts)
- [Message Types](#message-types)
- [Complete Code](#complete-code)

---

## Overview

The Moon framework supports two types of Services:

1. **Lua Service** - Implemented using Lua scripts, flexible and easy to use
2. **C++ Service** - Implemented in C++, higher performance

This document explains how to create a pure C++ Service that handles `PTYPE_TEXT` messages.

## File Structure

```
src/moon/
├── services/
│   ├── hello_service.h      # Header file
│   ├── hello_service.cpp    # Implementation file
│   └── lua_service.h/cpp    # Reference: Lua service implementation
├── core/
│   ├── service.hpp          # Service base class
│   ├── message.hpp          # Message structure
│   ├── config.hpp           # Message type definitions
│   └── server.h             # Server class
└── main.cpp                 # Service registration
```

---

## Step 1: Create Header File

Create `src/moon/services/hello_service.h`:

```cpp
#pragma once
#include "service.hpp"

class hello_service final: public moon::service {
public:
    hello_service() = default;
    ~hello_service();

private:
    // Required virtual functions to implement
    bool init(const moon::service_conf& conf) override;
    void dispatch(moon::message* msg) override;
};
```

### Key Points

| Element | Description |
|---------|-------------|
| `moon::service` | Base class that all services must inherit from |
| `init()` | Initialization function, called when the service is created |
| `dispatch()` | Message handler function, called when a message is received |
| `final` | Recommended to prevent further inheritance |

---

## Step 2: Implement the Service

Create `src/moon/services/hello_service.cpp`:

```cpp
#include "hello_service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"

using namespace moon;

// Destructor - log destruction
hello_service::~hello_service() {
    log::instance().logstring(
        true,
        moon::LogLevel::Info,
        moon::format("[WORKER %u] destroy service [%s]", worker_->id(), name().data()),
        id()
    );
}

// Initialization function
bool hello_service::init(const moon::service_conf& conf) {
    name_ = conf.name;

    log::instance().logstring(
        true,
        moon::LogLevel::Info,
        moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()),
        id()
    );

    // Handle unique service registration
    if (unique_ && !server_->set_unique_service(name_, id_)) {
        CONSOLE_ERROR("duplicate unique service name '%s'.", name_.data());
        return false;
    }

    ok_ = true;
    return ok_;
}

// Message dispatch handler
void hello_service::dispatch(moon::message* msg) {
    if (!ok())
        return;

    switch (msg->type) {
        case PTYPE_TEXT: {
            // Get message content
            std::string_view content;
            if (msg->is_bytes() && msg->data() != nullptr) {
                content = std::string_view(msg->data(), msg->size());
            }

            // Log the received message
            log::instance().logstring(
                true,
                moon::LogLevel::Info,
                moon::format("[%s] received: %s", name().data(), std::string(content).c_str()),
                id()
            );

            // If this is a call request (session > 0), send a response
            if (msg->session > 0) {
                std::string response = "Hello! You said: " + std::string(content);
                server_->response(msg->sender, response, msg->session, PTYPE_TEXT);
            }
            break;
        }
        case PTYPE_SHUTDOWN: {
            // Handle shutdown request
            get_server()->remove_service(id(), 0, 0);
            break;
        }
        default:
            break;
    }
}
```

### Core API Reference

#### Base Class Member Variables (inherited from `moon::service`)

| Variable | Type | Description |
|----------|------|-------------|
| `ok_` | `bool` | Whether the service is running normally |
| `unique_` | `bool` | Whether this is a unique service |
| `id_` | `uint32_t` | Service ID |
| `server_` | `server*` | Server pointer |
| `worker_` | `worker*` | Worker pointer |
| `name_` | `std::string` | Service name |

#### Message Structure (`moon::message`)

| Member | Type | Description |
|--------|------|-------------|
| `type` | `uint8_t` | Message type |
| `sender` | `uint32_t` | Sender Service ID |
| `receiver` | `uint32_t` | Receiver Service ID |
| `session` | `int64_t` | Session ID (>0 indicates response required) |
| `data()` | `const char*` | Message data pointer |
| `size()` | `size_t` | Message data length |
| `is_bytes()` | `bool` | Whether the data is byte data |

#### Sending a Response

```cpp
server_->response(
    uint32_t to,               // Target Service ID
    std::string_view content,  // Response content
    int64_t sessionid,         // Session ID
    uint8_t mtype = PTYPE_TEXT // Message type
);
```

---

## Step 3: Register the Service

Register the service in `src/moon/main.cpp`:

```cpp
#include "services/hello_service.h"

// ... Add registration code at the appropriate location ...

server_->register_service("lua", []() -> service_ptr_t {
    return std::make_unique<lua_service>();
});

// Register hello service
server_->register_service("hello", []() -> service_ptr_t {
    return std::make_unique<hello_service>();
});
```

### Registration Function Signature

```cpp
bool register_service(const std::string& type, register_func func);

// register_func definition
using register_func = service_ptr_t (*)();
using service_ptr_t = std::unique_ptr<service>;
```

---

## Step 4: Use the Service

### Calling from Lua

```lua
local moon = require "moon"

moon.async(function()
    -- Create hello service
    local hello_id = moon.new_service({
        stype = "hello",      -- Service type (corresponds to first param of register_service)
        name = "hello",       -- Service name
        source = "hello",     -- Source field
    })

    print("Hello service created with id:", hello_id)

    -- Wait for service creation to complete
    moon.sleep(100)

    -- Send a message (no response expected)
    moon.send("text", hello_id, "Hello from Lua!")

    -- Call and wait for response
    local response = moon.call("text", hello_id, "How are you?")
    print("Response:", response)

    moon.exit(0)
end)
```

### API Reference

| Function | Description |
|----------|-------------|
| `moon.new_service(conf)` | Create a service, `stype` specifies the type |
| `moon.send(ptype, id, data)` | Send a message without waiting for response |
| `moon.call(ptype, id, data)` | Send a message and wait for response |

---

## Core Concepts

### Service Lifecycle

```
┌─────────────────────────────────────────────────────────┐
│                    Service Lifecycle                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   moon.new_service()                                    │
│         │                                               │
│         ▼                                               │
│   ┌─────────────┐                                       │
│   │   Create    │  register_func() returns unique_ptr   │
│   │  Instance   │                                       │
│   └─────────────┘                                       │
│         │                                               │
│         ▼                                               │
│   ┌─────────────┐                                       │
│   │    init()   │  Initialize, return true on success   │
│   └─────────────┘                                       │
│         │                                               │
│         ▼                                               │
│   ┌─────────────┐                                       │
│   │  dispatch() │  Process messages in a loop           │
│   └─────────────┘                                       │
│         │                                               │
│         ▼                                               │
│   ┌─────────────┐                                       │
│   │ ~service()  │  Destructor, release resources        │
│   └─────────────┘                                       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Message Flow

```
┌──────────────┐    moon.send/call    ┌──────────────┐
│  Lua Service │ ─────────────────────▶ │ C++ Service  │
│              │                       │  dispatch()  │
│              │ ◀───────────────────── │              │
└──────────────┘   server_->response   └──────────────┘
```

---

## Message Types

Defined in `src/moon/core/config.hpp`:

| Constant | Value | Description |
|----------|-------|-------------|
| `PTYPE_UNKNOWN` | 0 | Unknown type |
| `PTYPE_SYSTEM` | 1 | System message |
| `PTYPE_TEXT` | 2 | Text message |
| `PTYPE_LUA` | 3 | Lua serialized message |
| `PTYPE_ERROR` | 4 | Error message |
| `PTYPE_DEBUG` | 5 | Debug message |
| `PTYPE_SHUTDOWN` | 6 | Shutdown message |
| `PTYPE_TIMER` | 7 | Timer message |
| `PTYPE_SOCKET_TCP` | 8 | TCP Socket message |
| `PTYPE_SOCKET_UDP` | 9 | UDP Socket message |
| `PTYPE_SOCKET_WS` | 10 | WebSocket message |
| `PTYPE_SOCKET_MOON` | 11 | Moon protocol message |
| `PTYPE_INTEGER` | 12 | Integer message |
| `PTYPE_LOG` | 13 | Log message |

---

## Complete Code

### hello_service.h

```cpp
#pragma once
#include "service.hpp"

class hello_service final: public moon::service {
public:
    hello_service() = default;
    ~hello_service();

private:
    bool init(const moon::service_conf& conf) override;
    void dispatch(moon::message* msg) override;
};
```

### hello_service.cpp

```cpp
#include "hello_service.h"
#include "message.hpp"
#include "server.h"
#include "worker.h"

using namespace moon;

hello_service::~hello_service() {
    log::instance().logstring(
        true,
        moon::LogLevel::Info,
        moon::format("[WORKER %u] destroy service [%s]", worker_->id(), name().data()),
        id()
    );
}

bool hello_service::init(const moon::service_conf& conf) {
    name_ = conf.name;

    log::instance().logstring(
        true,
        moon::LogLevel::Info,
        moon::format("[WORKER %u] new service [%s]", worker_->id(), name().data()),
        id()
    );

    if (unique_ && !server_->set_unique_service(name_, id_)) {
        CONSOLE_ERROR("duplicate unique service name '%s'.", name_.data());
        return false;
    }

    ok_ = true;
    return ok_;
}

void hello_service::dispatch(moon::message* msg) {
    if (!ok())
        return;

    switch (msg->type) {
        case PTYPE_TEXT: {
            std::string_view content;
            if (msg->is_bytes() && msg->data() != nullptr) {
                content = std::string_view(msg->data(), msg->size());
            }

            log::instance().logstring(
                true,
                moon::LogLevel::Info,
                moon::format("[%s] received: %s", name().data(), std::string(content).c_str()),
                id()
            );

            if (msg->session > 0) {
                std::string response = "Hello! You said: " + std::string(content);
                server_->response(msg->sender, response, msg->session, PTYPE_TEXT);
            }
            break;
        }
        case PTYPE_SHUTDOWN: {
            get_server()->remove_service(id(), 0, 0);
            break;
        }
        default:
            break;
    }
}
```

---

## Build and Run

```bash
# Build
premake5 build

# Run example
premake5 run example/example_hello.lua
```

---

## Extension Suggestions

1. **Add more message type handlers** - Add more cases in the `dispatch()` switch statement
2. **Add member variables** - Add state variables to the class
3. **Implement `signal()` method** - Handle signals (e.g., debug signals)
4. **Add timers** - Use `server_->timeout()` to set timers
