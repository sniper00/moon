---@meta
--- Moon Framework Core Library
--- This module provides the core functionality for the Moon framework, including:
--- - Service management and communication
--- - Coroutine-based asynchronous programming
--- - Message protocol handling
--- - Timer and scheduling utilities
--- - Global variable management
--- - Logging and debugging tools

-- Load base libraries for extended functionality
require("base.io")
require("base.os")
require("base.string")
require("base.table")
require("base.math")
require("base.util")
require("base.class")

-- Import core modules
local core             = require("moon.core")
local seri             = require("seri")

-- Localize frequently used functions for performance
local pairs            = pairs
local type             = type
local error            = error
local tremove          = table.remove
local traceback        = debug.traceback

-- Localize coroutine functions
local co_create        = coroutine.create
local co_running       = coroutine.running
local co_yield         = coroutine.yield
local co_resume        = coroutine.resume
local co_close         = coroutine.close

-- Localize core functions for better performance
local _send            = core.send
local _now             = core.now
local _addr            = core.id
local _timeout         = core.timeout
local _newservice      = core.new_service
local _queryservice    = core.queryservice
local _decode          = core.decode
local _scan_services   = core.scan_services

---@alias buffer_ptr lightuserdata
---@alias message_ptr lightuserdata

---@alias PTYPE
---| '"lua"' # Lua object messages (serialized)
---| '"text"' # Plain text messages
---| '"system"' # System control messages
---| '"error"' # Error messages
---| '"debug"' # Debug messages
---| '"shutdown"' # Shutdown signals
---| '"timer"' # Timer events
---| '"tcp"' # TCP socket messages
---| '"udp"' # UDP socket messages
---| '"websocket"' # WebSocket messages
---| '"moonsocket"' # MoonSocket messages
---| '"integer"' # Integer messages
---| '"log"' # Log messages

---@alias LogLevel
---| 1 # LOG_ERROR
---| 2 # LOG_WARN
---| 3 # LOG_INFO
---| 4 # LOG_DEBUG

---@class moon : core
--- Moon framework main object that extends the core functionality
--- Provides high-level APIs for service management, messaging, and asynchronous programming
local moon             = core

-- Protocol type constants for message communication
moon.PTYPE_SYSTEM      = 1  -- System control messages
moon.PTYPE_TEXT        = 2  -- Plain text messages
moon.PTYPE_LUA         = 3  -- Lua object messages (serialized)
moon.PTYPE_ERROR       = 4  -- Error messages
moon.PTYPE_DEBUG       = 5  -- Debug messages
moon.PTYPE_SHUTDOWN    = 6  -- Shutdown signals
moon.PTYPE_TIMER       = 7  -- Timer events
moon.PTYPE_SOCKET_TCP  = 8  -- TCP socket messages
moon.PTYPE_SOCKET_UDP  = 9  -- UDP socket messages
moon.PTYPE_SOCKET_WS   = 10 -- WebSocket messages
moon.PTYPE_SOCKET_MOON = 11 -- MoonSocket messages
moon.PTYPE_INTEGER     = 12 -- Integer messages
moon.PTYPE_LOG         = 13 -- Log messages

--moon.codecache = require("codecache")

--- Checks if debug logging is enabled
--- @return boolean @ True if debug level logging is active
moon.DEBUG             = function()
    return core.loglevel() == 4 -- LOG_DEBUG
end

-- Logging functions that map to different log levels
moon.error             = function(...) core.log(1, ...) end  -- Error level logging
moon.warn              = function(...) core.log(2, ...) end  -- Warning level logging
moon.info              = function(...) core.log(3, ...) end  -- Info level logging
moon.debug             = function(...) core.log(4, ...) end  -- Debug level logging

-- Serialization functions
moon.pack              = seri.pack    -- Pack Lua objects into binary format
moon.unpack            = seri.unpack  -- Unpack binary data into Lua objects

-- Global variable management
local _g               = _G

--- Override Lua's print function to use moon's logging system
--- All print() calls will now go through moon.info()
_g["print"]            = moon.info

--- Global variable export mechanism
--- Provides a controlled way to set global variables with warnings
--- It is not recommended to use this unless necessary.
moon.exports           = {}
setmetatable(
    moon.exports,
    {
        __newindex = function(_, name, value)
            rawset(_g, name, value)
        end,
        __index = function(_, name)
            return rawget(_g, name)
        end
    }
)

-- Prevent creation of unexpected global variables
-- This helps catch typos and enforce proper variable declaration
setmetatable(
    _g,
    {
        __newindex = function(_, name, value)
            if name:sub(1, 4) ~= 'sol.' then --ignore sol2 registered library
                local msg = string.format('USE "moon.exports.%s = <value>" INSTEAD OF SET GLOBAL VARIABLE', name)
                moon.error(traceback(msg, 2))
            else
                rawset(_g, name, value)
            end
        end
    }
)

-- Internal state management tables
local session_id_coroutine = {}  -- Maps session IDs to coroutines
local protocol = {}              -- Registered message protocols
local session_watcher = {}       -- Tracks session watchers for cleanup
local timer_routine = {}         -- Maps timer IDs to coroutines or functions
local timer_profile_trace = {}   -- Timer profiling information

--- Safely resumes a coroutine with error handling
--- @param co thread @ The coroutine to resume
--- @param ... any @ Arguments to pass to the coroutine
--- @return boolean, string? @ Success status and error message if failed
local function coresume(co, ...)
    local ok, err = co_resume(co, ...)
    if not ok then
        err = traceback(co, tostring(err))
        co_close(co)
        error(err)
    end
    return ok, err
end

--- Sends a message to the specified service. The message content will be packed according to the `PTYPE` type.
--- @param PTYPE PTYPE @ The protocol type, e.g., "lua", "text", "system"
--- @param receiver integer @ The service ID of the receiver
--- @param ... any @ The message content to be packed and sent
function moon.send(PTYPE, receiver, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end
    _send(p.PTYPE, receiver, p.pack(...), 0)
end

--- Sends a message to the specified service without packing the message content.
--- This is useful when you already have serialized data or want to send raw data.
--- @param PTYPE PTYPE @ The protocol type
--- @param receiver integer @ The service ID of the receiver
--- @param data? string|buffer_ptr @ The message content (raw data)
--- @param session? integer @ The session ID for request-response pattern
--- @param sender? integer @ The dummy sender's service ID
--- @return integer @ The session ID used for the message
function moon.raw_send(PTYPE, receiver, data, session, sender)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end

    session = session or 0

    return _send(p.PTYPE, receiver, data, session, sender)
end

---@class service_params
--- Configuration parameters for creating a new service
---@field name string The name of the service
---@field file string The path to the startup script file for the service
---@field unique? boolean An optional boolean that indicates whether the service is unique. The default is `false`. If set to `true`, you can use the `moon.query(name)` function to query the service ID.
---@field threadid? integer Represents the ID of the worker thread where the service is running. The default value is 0, and the service will be added to the current worker thread with the fewest number of services. If set to a non-zero value, the service will be created in the specified worker thread.

---@class protocol_config
--- Configuration for registering a message protocol
---@field name string The protocol name
---@field PTYPE integer The protocol type constant
---@field pack? fun(...: any): string|buffer_ptr The packing function
---@field unpack? fun(data: string|buffer_ptr, len?: integer): ...any The unpacking function
---@field dispatch? fun(sender: integer, session: integer, ...: any) The message handler
---@field israw? boolean Whether this is a raw protocol (receives message_ptr directly)

--- Creates a new service asynchronously.
--- The service will be created in a separate worker thread and can communicate with other services.
--- @async
--- @param params service_params @ The configuration for creating the service. In addition to the basic configuration, it can also be used to pass additional parameters to the newly created service.
--- @return integer @ Returns the ID of the created service. If the ID is 0, it means the service creation failed.
function moon.new_service(params)
    return moon.wait(_newservice(params))
end

--- Terminates the current service gracefully.
--- It closes all coroutines associated with the service except the one that is currently running.
--- After closing the coroutines, it kills the service.
--- This function should be called when you want to shut down a service cleanly.
function moon.quit()
    local running = co_running()
    -- Close all session-related coroutines
    for k, co in pairs(session_id_coroutine) do
        if type(co) == "thread" and co ~= running then
            co_close(co)
            session_id_coroutine[k] = false
        end
    end

    -- Close all timer-related coroutines
    for k, co in pairs(timer_routine) do
        if type(co) == "thread" and co ~= running then
            co_close(co)
            timer_routine[k] = false
        end
    end

    moon.kill(_addr)
end

--- Get the ID of a **`unique service`** based on the service `name`
--- Unique services are services that can be looked up by name across the entire system.
--- @param name string @ The name of the unique service
--- @return integer @ The service ID, or 0 if the service does not exist
function moon.queryservice(name)
    if type(name) == 'string' then
        return _queryservice(name)
    end
    return name
end

--- Packs a Lua object into a string and stores it in the moon's environment.
--- The environment is shared across all services and persists for the lifetime of the server.
--- @param name string @ The name of the object in the environment
--- @param ... any @ The Lua object(s) to be packed and stored
--- @return string @ The packed data string
function moon.env_packed(name, ...)
    return core.env(name, seri.packs(...))
end

--- Retrieves a Lua object stored in the moon's environment and unpacks it.
--- @param name string @ The name of the object in the environment
--- @return any @ The unpacked Lua object
function moon.env_unpacked(name)
    return seri.unpack(core.env(name))
end

--- Retrieves the current server UTC timestamp.
--- @return integer @ Unix timestamp in seconds
function moon.time()
    return _now(1000)
end

--- Retrieves the command-line arguments passed at the start of the process.
--- Example usage:
--- ```shell
--- ./moon main.lua arg1 arg2 arg3
--- ```
--- This will return `{arg1, arg2, arg3}`.
--- @return string[] @ An array of the command-line arguments
function moon.args()
    return load(moon.env("ARG"))()
end

------------------------- Coroutine Operations -------------------------

-- Coroutine pool management
local co_num = 0  -- Number of currently running coroutines

-- Coroutine pool for reuse to avoid creating new coroutines frequently
local co_pool = setmetatable({}, { __mode = "kv" })

--- Executes a function and manages coroutine lifecycle
--- @param co thread @ The coroutine to execute
--- @param fn function @ The function to execute
--- @param ... any @ Arguments to pass to the function
local function invoke(co, fn, ...)
    co_num = co_num + 1
    fn(...)
    co_num = co_num - 1
    co_pool[#co_pool + 1] = co
end

--- Main coroutine routine that handles function execution and yielding
--- @async
--- @param fn function @ The function to execute
--- @param ... any @ Arguments to pass to the function
local function routine(fn, ...)
    local co = co_running()
    invoke(co, fn, ...)
    while true do
        invoke(co, co_yield())
    end
end

--- Creates a new coroutine and immediately starts executing it.
--- Functions marked with `async` need to be called within `moon.async`.
--- If the `fn` function does not call `coroutine.yield`, it will be executed synchronously.
--- 
--- Example usage:
--- ```lua
--- local function foo(a, b)
---     print("start foo", a, b)
---     moon.sleep(1000)
---     print("end foo", a, b)
--- end
--- local function bar(a, b)
---     print("start bar", a, b)
---     moon.sleep(500)
---     print("end bar", a, b)
--- end
--- moon.async(foo, 1, 2)
--- moon.async(bar, 3, 4)
--- ```
--- @param fn async fun(...: any) @ The function to be executed asynchronously
--- @param ... any @ Optional parameters, passed to the `fn` function
--- @return thread @ The newly created coroutine
function moon.async(fn, ...)
    local co = tremove(co_pool) or co_create(routine)
    coresume(co, fn, ...)
    return co
end

--- Suspends the current coroutine and waits for a message or wakeup.
--- This is the core function for asynchronous programming in Moon.
--- 
--- When called with a session ID, the coroutine will be resumed when a message
--- with that session ID is received. When called without parameters, it can
--- be used to yield control to other coroutines.
--- @async
--- @param session? integer @ An optional session ID used to map the coroutine for wakeup
--- @param receiver? integer @ An optional receiver's service ID
--- @return any ... @ Returns the unpacked message if the coroutine is resumed by a message. If the coroutine is resumed by `moon.wakeup`, it returns the additional parameters passed by `moon.wakeup`. If the coroutine is broken, it returns `false` and "BREAK".
function moon.wait(session, receiver)
    if session then
        session_id_coroutine[session] = co_running()
        if receiver then
            session_watcher[session] = receiver
        end
    else
        if type(receiver) == "string" then -- receiver is error message
            return false, receiver
        end
    end

    local a, b, c = co_yield()
    if a then
        -- sz,len,PTYPE - Message received
        return protocol[c].unpack(a, b)
    else
        -- false, "BREAK", {...} - Wakeup or error
        if session then
            session_id_coroutine[session] = false
        end

        if c then -- Extra parameters passed to moon.wakeup
            return table.unpack(c)
        else
            return a, b --- false, "BREAK"
        end
    end
end

--- Manually resumes a suspended coroutine.
--- This is useful for implementing custom scheduling or breaking out of waits.
--- @param co thread @ The coroutine to be resumed
--- @param ... any @ Optional parameters that will be returned by `moon.wait` when the coroutine is resumed
function moon.wakeup(co, ...)
    local args = { ... }
    moon.timeout(0, function()
        local ok, err = co_resume(co, false, "BREAK", args)
        if not ok then
            err = traceback(co, tostring(err))
            co_close(co)
            moon.error(err)
        end
    end)
end

--- Retrieves the count of running coroutines and the total number of coroutines in the coroutine pool.
--- This is useful for monitoring system performance and debugging.
--- @return integer, integer @ The first integer is the count of running coroutines. The second integer is the total number of coroutines in the coroutine pool.
function moon.coroutine_num()
    return co_num, #co_pool
end

------------------------------------------

--- Get all service names and IDs in the specified thread, in JSON format
--- This function is useful for debugging and monitoring service distribution across worker threads.
--- @async
--- @param workerid integer @ The worker thread ID
--- @return string @ JSON string containing service information
function moon.scan_services(workerid)
    return moon.wait(_scan_services(workerid))
end

--- Sends a message to the target service and waits for a response.
--- This implements a request-response pattern where the receiver must call `moon.response` to return the result.
--- 
--- - If the request is successful, the return value is the `params` part of `moon.response(id, response, params...)`.
--- - If the request fails, it returns `false` and an error message string.
--- @async
--- @param PTYPE PTYPE @ The protocol type
--- @param receiver integer @ The service ID of the receiver
--- @param ... any @ The message content to send
--- @return any ... @ The response from the receiver
--- @nodiscard
function moon.call(PTYPE, receiver, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon call unknown PTYPE[%s] message", PTYPE))
    end

    if receiver == 0 then
        error("moon call receiver == 0")
    end

    return moon.wait(_send(p.PTYPE, receiver, p.pack(...)))
end

--- Responds to a request from `moon.call`.
--- This function should be called by the service that received a call to send back a response.
--- @param PTYPE PTYPE @ The protocol type
--- @param receiver integer @ The service ID of the receiver
--- @param sessionid integer @ The session ID from the original call
--- @param ... any @ The response content
function moon.response(PTYPE, receiver, sessionid, ...)
    if sessionid == 0 then return end
    local p = protocol[PTYPE]
    if not p then
        error("handle unknown message")
    end

    if receiver == 0 then
        error("moon response receiver == 0")
    end

    _send(p.PTYPE, receiver, p.pack(...), sessionid)
end

------------------------------------
--- Internal message dispatcher function
--- Handles incoming messages and routes them to appropriate handlers
--- @param PTYPE integer @ The protocol type
--- @param sender integer @ The sender's service ID
--- @param session integer @ The session ID
--- @param sz string @ The message data
--- @param len integer @ The message length
--- @param m message_ptr @ The raw message pointer
local function _dispatch(PTYPE, sender, session, sz, len, m)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("handle unknown PTYPE: %s. sender %u", PTYPE, sender))
    end

    if session > 0 then
        -- Handle response messages (request-response pattern)
        session_watcher[session] = nil
        local co = session_id_coroutine[session]
        session_id_coroutine[session] = nil
        if co then
            --print(coroutine.status(co))
            coresume(co, sz, len, PTYPE)
            --print(coroutine.status(co))
            return
        end

        if co ~= false then
            error(string.format("%s: response [%u] can not find co.", moon.name, session))
        end
    else
        -- Handle regular messages
        local dispatch = p.dispatch
        if not dispatch then
            error(string.format("[%s] dispatch PTYPE [%u] is nil", moon.name, p.PTYPE))
            return
        end

        if not p.israw then
            -- Create new coroutine for message handling
            local co = tremove(co_pool) or co_create(routine)
            if not p.unpack then
                error(string.format("PTYPE %s has no unpack function.", p.PTYPE))
            end
            coresume(co, dispatch, sender, session, p.unpack(sz, len))
        else
            -- Handle raw messages directly
            dispatch(m)
        end
    end
end

-- Register the dispatch function with the core
core.callback(_dispatch)

--- Registers a new message protocol.
--- Protocols define how messages are packed, unpacked, and dispatched.
--- @param t protocol_config @ Protocol configuration table
function moon.register_protocol(t)
    local PTYPE = t.PTYPE
    if protocol[PTYPE] then
        print("Warning attemp register duplicated PTYPE", t.name)
    end
    protocol[PTYPE] = t
    protocol[t.name] = t
end

local reg_protocol = moon.register_protocol

--- Sets the message handler for the specified protocol type.
--- The handler function will receive unpacked message data.
--- @param PTYPE PTYPE @ The protocol type
--- @param fn fun(sender: integer, session: integer, ...: any) @ The message handler function
function moon.dispatch(PTYPE, fn)
    local p = protocol[PTYPE]
    if fn then
        p.dispatch = fn
    end
end

--- Sets the message handler for the specified protocol type.
--- Unlike `moon.dispatch`, this function does not unpack the message and receives the raw message pointer.
--- @param PTYPE PTYPE @ The protocol type
--- @param fn fun(m: message_ptr) @ The message handler function that receives raw message
function moon.raw_dispatch(PTYPE, fn)
    local p = protocol[PTYPE]
    if fn then
        p.dispatch = fn
        p.israw = true
    end
end

-- Register built-in protocols

--- Lua object protocol - for sending serialized Lua objects
reg_protocol {
    name = "lua",
    PTYPE = moon.PTYPE_LUA,
    pack = moon.pack,
    unpack = moon.unpack,
    dispatch = function()
        error("PTYPE_LUA dispatch not implemented")
    end
}

--- Text protocol - for sending plain text messages
reg_protocol {
    name = "text",
    PTYPE = moon.PTYPE_TEXT,
    pack = function(...)
        return ...
    end,
    unpack = moon.tostring,
    dispatch = function()
        error("PTYPE_TEXT dispatch not implemented")
    end
}

--- Integer protocol - for sending integer values
reg_protocol {
    name = "integer",
    PTYPE = moon.PTYPE_INTEGER,
    pack = function(...)
        return ...
    end,
    unpack = function(nval)
        return nval
    end,
    dispatch = function()
        error("PTYPE_TEXT dispatch not implemented")
    end
}

--- Error protocol - for sending error messages
reg_protocol {
    name = "error",
    PTYPE = moon.PTYPE_ERROR,
    pack = function(...)
        return ...
    end,
    unpack = function(sz, len)
        local data = moon.tostring(sz, len) or "unknown error"
        return false, data
    end,
    dispatch = function(_, _, ...)
        moon.error(...)
    end
}

-- System command handlers
local system_command = {}

--- Handles service exit notifications
--- @param sender integer @ The service ID that exited
--- @param what string @ The exit reason
system_command._service_exit = function(sender, what)
    for k, v in pairs(session_watcher) do
        if v == sender then
            local co = session_id_coroutine[k]
            if co then
                session_id_coroutine[k] = nil
                coresume(co, false, what)
                return
            end
        end
    end
end

--- Registers a system command handler
--- @param cmd string @ The command name
--- @param fn fun(sender: integer, ...: any) @ The handler function
moon.system = function(cmd, fn)
    system_command[cmd] = fn
end

--- System protocol - for internal system control messages
reg_protocol {
    name = "system",
    PTYPE = moon.PTYPE_SYSTEM,
    israw = true,
    pack = function(...)
        return table.concat({ ... }, ",")
    end,
    dispatch = function(msg)
        local sender, data = _decode(msg, "SZ")
        local params = string.split(data, ',')
        local func = system_command[params[1]]
        if func then
            func(sender, table.unpack(params, 2))
        end
    end
}

--- TCP socket protocol - for TCP socket messages
reg_protocol {
    name = "tcp",
    PTYPE = moon.PTYPE_SOCKET_TCP,
    pack = function(...)
        return ...
    end,
    unpack = moon.tostring,
    dispatch = function()
        error("PTYPE_SOCKET_TCP dispatch not implemented")
    end
}

--- UDP socket protocol - for UDP socket messages
reg_protocol {
    name = "udp",
    PTYPE = moon.PTYPE_SOCKET_UDP,
    pack = function(...) return ... end,
    dispatch = function(_)
        error("PTYPE_SOCKET_UDP dispatch not implemented")
    end
}

--- WebSocket protocol - for WebSocket messages
reg_protocol {
    name = "websocket",
    PTYPE = moon.PTYPE_SOCKET_WS,
    pack = function(...) return ... end,
    dispatch = function(_)
        error("PTYPE_SOCKET_WS dispatch not implemented")
    end
}

--- MoonSocket protocol - for MoonSocket messages
reg_protocol {
    name = "moonsocket",
    PTYPE = moon.PTYPE_SOCKET_MOON,
    pack = function(...) return ... end,
    dispatch = function()
        error("PTYPE_SOCKET_MOON dispatch not implemented")
    end
}

-- Shutdown handling
local cb_shutdown

--- Shutdown protocol - for graceful shutdown signals
reg_protocol {
    name = "shutdown",
    PTYPE = moon.PTYPE_SHUTDOWN,
    israw = true,
    dispatch = function()
        if cb_shutdown then
            cb_shutdown()
        else
            local name = moon.name
            --- bootstrap or not unique service
            if name == "bootstrap" or 0 == moon.queryservice(moon.name) then
                moon.quit()
            end
        end
    end
}

--- Registers a process exit signal handler function.
--- You need to actively call `moon.quit` in the handler, otherwise the service will not exit.
--- You can start a new coroutine to execute asynchronous logic: such as the server safe shutdown process, waiting for services to close in a specified order, saving data, etc.
--- **For unique services, you generally need to register this function to handle the exit process, or use `moon.kill` to force close**
--- @param callback fun() @ The function to be called when the process is shutting down
function moon.shutdown(callback)
    cb_shutdown = callback
end

-------------------------- Timer Management --------------------------

--- Timer protocol - for timer event messages
reg_protocol {
    name = "timer",
    PTYPE = moon.PTYPE_TIMER,
    israw = true,
    dispatch = function(m)
        local timerid = _decode(m, "C")
        local v = timer_routine[timerid]
        timer_routine[timerid] = nil
        local trace = timer_profile_trace[timerid]
        timer_profile_trace[timerid] = nil
        if not v then
            return
        end
        local st = moon.clock()
        if type(v) == "thread" then
            coresume(v, timerid)
        else
            v()
        end
        local elapsed = moon.clock() - st
        if trace and elapsed > 0.1 then
            moon.warn(string.format("Timer %s cost %ss trace '%s'", timerid, elapsed, trace))
        end
    end
}

--- Removes a timer.
--- @param timerid integer @ The ID of the timer to be removed
function moon.remove_timer(timerid)
    timer_routine[timerid] = false
end

--- Creates a timer that triggers a callback function after waiting for a specified number of milliseconds.
--- If `mills <= 0`, the behavior of this function degenerates into posting a message to the message queue,
--- which is very useful for operations that need to be delayed.
--- @param mills integer @ The number of milliseconds to wait
--- @param fn fun(timerid: integer) @ The callback function to be triggered
--- @param profile_trace? string @ Trace for timer profile (useful for debugging slow timers)
--- @return integer @ Returns the timer ID. You can use `moon.remove_timer` to remove the timer
function moon.timeout(mills, fn, profile_trace)
    local timerid = _timeout(mills)
    timer_routine[timerid] = fn
    timer_profile_trace[timerid] = profile_trace
    return timerid
end

--- Suspends the current coroutine for at least `mills` milliseconds.
--- This is the primary way to implement delays in Moon applications.
--- @async
--- @param mills integer @ The number of milliseconds to suspend
--- @param profile_trace? string @ Trace for timer profile (useful for debugging slow sleeps)
--- @return boolean, string? @ If the timer is awakened by `moon.wakeup`, it returns `false`. If the timer is triggered normally, it returns `true`.
function moon.sleep(mills, profile_trace)
    local timerid = _timeout(mills)
    timer_routine[timerid] = co_running()
    timer_profile_trace[timerid] = profile_trace
    local id, reason = co_yield()
    if id ~= timerid then
        timer_routine[timerid] = false
        return false, reason
    end
    return true
end

-------------------------- Debug Utilities ----------------------------

-- Debug command handlers
local debug_command = {}

--- Garbage collection command
--- @return integer @ Memory usage in KB after garbage collection
debug_command.gc = function()
    collectgarbage("collect")
    return collectgarbage("count")
end

--- Memory usage command
--- @return integer @ Current memory usage in KB
debug_command.mem = function()
    return collectgarbage("count")
end

--- Ping command for testing connectivity
--- @return string @ Always returns "pong"
debug_command.ping = function()
    return "pong"
end

--- System state command
--- @return string @ Formatted string with coroutine and CPU information
debug_command.state = function()
    local running_num, free_num = moon.coroutine_num()
    return string.format("coroutine: running %d free %d. cpu:%d", running_num, free_num, moon.cpu())
end

--- Debug protocol - for remote debugging commands
reg_protocol {
    name = "debug",
    PTYPE = moon.PTYPE_DEBUG,
    pack = moon.pack,
    unpack = moon.unpack,
    dispatch = function(sender, session, cmd, ...)
        local func = debug_command[cmd]
        if func then
            moon.response("debug", sender, session, func(sender, session, ...))
        else
            moon.response("debug", sender, session, "unknow debug cmd " .. cmd)
        end
    end
}

--- Log protocol - for remote logging
reg_protocol {
    name = "log",
    PTYPE = moon.PTYPE_LOG,
    pack = function(...)
        return ...
    end,
    unpack = function(sz, len)
        local s = moon.tostring(sz, len)
        return math.tointeger(string.sub(s, 1, 1)), string.sub(s, 2)
    end,
    dispatch = function(_, _, ...)
        core.log(...)
    end
}

return moon
