require("base.io")
require("base.os")
require("base.string")
require("base.table")
require("base.math")
require("base.util")
require("base.class")

local core = require("moon.core")
local seri = require("seri")

local pairs = pairs
local type = type
local error = error
local tremove = table.remove
local traceback = debug.traceback

local co_create = coroutine.create
local co_running = coroutine.running
local co_yield = coroutine.yield
local co_resume = coroutine.resume
local co_close = coroutine.close

local _send = core.send
local _now = core.now
local _addr = core.id
local _timeout = core.timeout
local _newservice = core.new_service
local _queryservice = core.queryservice
local _decode = core.decode
local _scan_services = core.scan_services

---@class moon : core
local moon = core

moon.PTYPE_SYSTEM = 1
moon.PTYPE_TEXT = 2
moon.PTYPE_LUA = 3
moon.PTYPE_ERROR = 4
moon.PTYPE_DEBUG = 5
moon.PTYPE_SHUTDOWN = 6
moon.PTYPE_TIMER = 7
moon.PTYPE_SOCKET_TCP = 8
moon.PTYPE_SOCKET_UDP = 9
moon.PTYPE_SOCKET_WS = 10
moon.PTYPE_SOCKET_MOON = 11
moon.PTYPE_INTEGER = 12
moon.PTYPE_LOG = 13

--moon.codecache = require("codecache")

-- LOG_ERROR = 1
-- LOG_WARN = 2
-- LOG_INFO = 3
-- LOG_DEBUG = 4
moon.DEBUG = function()
    return core.loglevel() == 4 -- LOG_DEBUG
end
moon.error = function(...) core.log(1, ...) end
moon.warn = function(...) core.log(2, ...) end
moon.info = function(...) core.log(3, ...) end
moon.debug = function(...) core.log(4, ...) end

moon.pack = seri.pack
moon.unpack = seri.unpack

--export global variable
local _g = _G

---rewrite lua print
_g["print"] = moon.info

--- Sets a Lua global variable. It is not recommended to use this unless necessary.
moon.exports = {}
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

-- Disable create unexpected global variable
setmetatable(
    _g,
    {
        __newindex = function(_, name, value)
            if name:sub(1, 4) ~= 'sol.' then --ignore sol2 registed library
                local msg = string.format('USE "moon.exports.%s = <value>" INSTEAD OF SET GLOBAL VARIABLE', name)
                moon.error(traceback(msg, 2))
            else
                rawset(_g, name, value)
            end
        end
    }
)

local session_id_coroutine = {}
local protocol = {}
local session_watcher = {}
local timer_routine = {}
local timer_profile_trace = {}

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
--- @param PTYPE string @ The protocol type, e.g., "lua".
--- @param receiver integer @ The service ID of the receiver.
--- @param ... any @ The message content.
function moon.send(PTYPE, receiver, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end
    _send(p.PTYPE, receiver, p.pack(...), 0)
end

--- Sends a message to the specified service without packing the message content.
--- @param PTYPE string @ The protocol type.
--- @param receiver integer @ The service ID of the receiver.
--- @param data? string|buffer_ptr @ The message content.
--- @param session? integer @ The session ID.
function moon.raw_send(PTYPE, receiver, data, session)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end

    session = session or 0

    return _send(p.PTYPE, receiver, data, session)
end

---@class service_params
---@field name string The name of the service.
---@field file string The path to the startup script file for the service.
---@field unique? boolean An optional boolean that indicates whether the service is unique. The default is `false`. If set to `true`, you can use the `moon.query(name)` function to query the service ID.
---@field threadid? integer Represents the ID of the worker thread where the service is running. The default value is 0, and the service will be added to the current worker thread with the fewest number of services. If set to a non-zero value, the service will be created in the specified worker thread.

--- Creates a new service.
--- @async
--- @param params service_params The configuration for creating the service. In addition to the basic configuration, it can also be used to pass additional parameters to the newly created service.
--- @return integer Returns the ID of the created service. If the ID is 0, it means the service creation failed.
function moon.new_service(params)
    return moon.wait(_newservice(params))
end

--- Terminates the current service. It closes all coroutines associated with the service except the one that is currently running.
--- After closing the coroutines, it kills the service.
function moon.quit()
    local running = co_running()
    for k, co in pairs(session_id_coroutine) do
        if type(co) == "thread" and co ~= running then
            co_close(co)
            session_id_coroutine[k] = false
        end
    end

    for k, co in pairs(timer_routine) do
        if type(co) == "thread" and co ~= running then
            co_close(co)
            timer_routine[k] = false
        end
    end

    moon.kill(_addr)
end

--- Get the ID of a **`unique service`** based on the service `name`
---@param name string
---@return integer @ 0 indicates the service does not exist
function moon.queryservice(name)
    if type(name) == 'string' then
        return _queryservice(name)
    end
    return name
end

--- Packs a Lua object into a string and stores it in the moon's environment.
--- @param name string @ The name of the object in the environment.
--- @param ... any @ The Lua object(s) to be packed.
function moon.env_packed(name, ...)
    return core.env(name, seri.packs(...))
end

--- Retrieves a Lua object stored in the moon's environment and unpacks it.
--- @param name string @ The name of the object in the environment.
--- @return any @ The unpacked Lua object.
function moon.env_unpacked(name)
    return seri.unpack(core.env(name))
end

--- Retrieves the current server UTC timestamp.
--- @return integer @ Unix timestamp in seconds
function moon.time()
    return _now(1000)
end

--- Retrieves the command-line arguments passed at the start of the process. For example:
--- ```shell
--- ./moon main.lua arg1 arg2 arg3
--- ```
--- This will return `{arg1, arg2, arg3}`.
---
---@return string[] @An array of the command-line arguments
function moon.args()
    return load(moon.env("ARG"))()
end

-------------------------协程操作封装--------------------------

local co_num = 0

local co_pool = setmetatable({}, { __mode = "kv" })

local function invoke(co, fn, ...)
    co_num = co_num + 1
    fn(...)
    co_num = co_num - 1
    co_pool[#co_pool + 1] = co
end

local function routine(fn, ...)
    local co = co_running()
    invoke(co, fn, ...)
    while true do
        invoke(co, co_yield())
    end
end

--- Creates a new coroutine and immediately starts executing it. Functions marked with `async` need to be called within `moon.async`. If the `fn` function does not call `coroutine.yield`, it will be executed synchronously.
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
---
---@param fn fun(...) @The function to be executed asynchronously
---@param ... any @Optional parameters, passed to the `fn` function
---@return thread @The newly created coroutine
function moon.async(fn, ...)
    local co = tremove(co_pool) or co_create(routine)
    coresume(co, fn, ...)
    return co
end

--- Suspends the current coroutine.
--- @async
--- @param session? integer @ An optional session ID used to map the coroutine for wakeup.
--- @param receiver? integer @ An optional receiver's service ID.
--- @return ... @ Returns the unpacked message if the coroutine is resumed by a message. If the coroutine is resumed by `moon.wakeup`, it returns the additional parameters passed by `moon.wakeup`. If the coroutine is broken, it returns `false` and "BREAK".
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
        -- sz,len,PTYPE
        return protocol[c].unpack(a, b)
    else
        -- false, "BREAK", {...}
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
--- @param co thread @ The coroutine to be resumed.
--- @param ... any @ Optional parameters that will be returned by `moon.wait` when the coroutine is resumed.
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
--- @return integer, integer @ The first integer is the count of running coroutines. The second integer is the total number of coroutines in the coroutine pool.
function moon.coroutine_num()
    return co_num, #co_pool
end

------------------------------------------

--- Get all service names and IDs in the specified thread, in JSON format
---@async
---@param workerid integer @ The worker thread ID.
---@return string
function moon.scan_services(workerid)
    return moon.wait(_scan_services(workerid))
end

--- Sends a message to the target service and waits for a response. The receiver must call `moon.response` to return the result.
---  - If the request is successful, the return value is the `params` part of `moon.response(id, response, params...)`.
---  - If the request fails, it returns `false` and an error message string.
---@async
---@param PTYPE string @ The protocol type.
---@param receiver integer @ The service ID of the receiver.
---@return ... @ The response from the receiver.
---@nodiscard
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
--- @param PTYPE string @ The protocol type.
--- @param receiver integer @ The service ID of the receiver.
--- @param sessionid integer @ The session ID.
--- @param ... any @ The response content.
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
---@param m message_ptr
---@param PTYPE string
local function _dispatch(PTYPE, sender, session, sz, len, m)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("handle unknown PTYPE: %s. sender %u", PTYPE, sender))
    end

    if session > 0 then
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
        local dispatch = p.dispatch
        if not dispatch then
            error(string.format("[%s] dispatch PTYPE [%u] is nil", moon.name, p.PTYPE))
            return
        end

        if not p.israw then
            local co = tremove(co_pool) or co_create(routine)
            if not p.unpack then
                error(string.format("PTYPE %s has no unpack function.", p.PTYPE))
            end
            coresume(co, dispatch, sender, session, p.unpack(sz, len))
        else
            dispatch(m)
        end
    end
end

core.callback(_dispatch)

--- Registers a new message protocol.
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
--- @param PTYPE string @ The protocol type.
--- @param fn fun(sender:integer, session:integer, ...) @ The message handler
function moon.dispatch(PTYPE, fn)
    local p = protocol[PTYPE]
    if fn then
        p.dispatch = fn
    end
end

--- Sets the message handler for the specified protocol type. Unlike `moon.dispatch`, this function does not unpack the message.
--- @param PTYPE string @ The protocol type.
--- @param fn fun(m:message_ptr) The message handler.
function moon.raw_dispatch(PTYPE, fn)
    local p = protocol[PTYPE]
    if fn then
        p.dispatch = fn
        p.israw = true
    end
end

reg_protocol {
    name = "lua",
    PTYPE = moon.PTYPE_LUA,
    pack = moon.pack,
    unpack = moon.unpack,
    dispatch = function()
        error("PTYPE_LUA dispatch not implemented")
    end
}

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

local system_command = {}

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

moon.system = function(cmd, fn)
    system_command[cmd] = fn
end

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

reg_protocol {
    name = "udp",
    PTYPE = moon.PTYPE_SOCKET_UDP,
    pack = function(...) return ... end,
    dispatch = function(_)
        error("PTYPE_SOCKET_UDP dispatch not implemented")
    end
}

reg_protocol {
    name = "websocket",
    PTYPE = moon.PTYPE_SOCKET_WS,
    pack = function(...) return ... end,
    dispatch = function(_)
        error("PTYPE_SOCKET_WS dispatch not implemented")
    end
}

reg_protocol {
    name = "moonsocket",
    PTYPE = moon.PTYPE_SOCKET_MOON,
    pack = function(...) return ... end,
    dispatch = function()
        error("PTYPE_SOCKET_MOON dispatch not implemented")
    end
}

local cb_shutdown

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

--- Registers a process exit signal handler function. You need to actively call `moon.quit` in the handler, otherwise the service will not exit.
--- You can start a new coroutine to execute asynchronous logic: such as the server safe shutdown process, waiting for services to close in a specified order, saving data, etc.
--- **For unique services, you generally need to register this function to handle the exit process, or use `moon.kill` to force close**
---@param callback fun() @ The function to be called when the process is shutting down.
function moon.shutdown(callback)
    cb_shutdown = callback
end

--------------------------timer-------------

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
--- @param timerid integer @ The ID of the timer to be removed.
function moon.remove_timer(timerid)
    timer_routine[timerid] = false
end

--- Creates a timer that triggers a callback function after waiting for a specified number of milliseconds. If `mills <= 0`, the behavior of this function degenerates into posting a message to the message queue, which is very useful for operations that need to be delayed.
--- @param mills integer @ The number of milliseconds to wait.
--- @param fn function @ The callback function to be triggered.
--- @param profile_trace? string @ Trace for timer profile.
--- @return integer @ Returns the timer ID. You can use `moon.remove_timer` to remove the timer.
function moon.timeout(mills, fn, profile_trace)
    local timerid = _timeout(mills)
    timer_routine[timerid] = fn
    timer_profile_trace[timerid] = profile_trace
    return timerid
end

--- Suspends the current coroutine for at least `mills` milliseconds.
--- @async
--- @param mills integer @ The number of milliseconds to suspend.
--- @param profile_trace? string @ Trace for timer profile.
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

--------------------------DEBUG----------------------------

local debug_command = {}

debug_command.gc = function()
    collectgarbage("collect")
    return collectgarbage("count")
end

debug_command.mem = function()
    return collectgarbage("count")
end

debug_command.ping = function()
    return "pong"
end

debug_command.state = function()
    local running_num, free_num = moon.coroutine_num()
    return string.format("coroutine: running %d free %d. cpu:%d", running_num, free_num, moon.cpu())
end

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
