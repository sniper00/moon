require("base.io")
require("base.os")
require("base.string")
require("base.table")
require("base.math")
require("base.util")
require("base.class")

---@class core
local core = require("moon.api")
local json = require("json")
local seri = require("seri")

local pairs = pairs
local type = type
local setmetatable = setmetatable
local tremove = table.remove

local jencode = json.encode

local co_create = coroutine.create
local co_running = coroutine.running
local co_yield = coroutine.yield
local co_resume = coroutine.resume

local _send = core.send

local PTYPE_SYSTEM = 1
local PTYPE_TEXT = 2
local PTYPE_LUA = 3
local PTYPE_SOCKET = 4
local PTYPE_ERROR = 5
local PTYPE_SOCKET_WS = 6


---@class moon : core
local moon = {
    PTYPE_TEXT = PTYPE_TEXT,
    PTYPE_LUA = PTYPE_LUA,
    PTYPE_SOCKET = PTYPE_SOCKET,
    PTYPE_SOCKET_WS = PTYPE_SOCKET_WS
}

setmetatable(moon, {__index = core})

--export global variable
local _g = _G
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

-- disable create unexpected global variable
setmetatable(
    _g,
    {
        __newindex = function(_, name,value)
            if name:sub(1,4)~='sol.' then --ignore sol2 registed library
                local msg = string.format('USE "moon.exports.%s = <value>" INSTEAD OF SET GLOBAL VARIABLE', name)
                print(debug.traceback(msg, 2))
                print("")
            else
                rawset(_g, name, value)
            end
        end
    }
)

moon.add_package_path = function(p)
    package.path = package.path .. p
end

local sid_ = core.id()

local uuid = 1
local session_id_coroutine = {}
local protocol = {}
local session_watcher = {}

local function coresume(co, ...)
    local ok, err = co_resume(co, ...)
    if not ok then
        error(debug.traceback(co, err))
    end
end

---make map<coroutine,sessionid>
local function make_response(receiver)
    repeat
        uuid = uuid + 1
        if uuid == 0xFFFFFFF then
            uuid = 1
        end
    until nil == session_id_coroutine[uuid]

    if receiver then
        session_watcher[uuid] = receiver
    end

    local co = co_running()
    session_id_coroutine[uuid] = co
    return uuid
end

--- 取消等待session的回应
function moon.cancel_session(sessionid)
    session_id_coroutine[sessionid] = false
end

moon.make_response = make_response

---
---注册服务启动回掉函数,这个函数的调用时机:
---1.如果服务是在配置文件中配置的服务，回掉函数会在所有配置的服务
---被创建之后调用。所以可以在回掉函数内查询其它唯一服务的 serviceid。
---2.运行时动态创建的服务，第一次update之前调用
---@param callback fun()
function moon.start(callback)
    core.set_cb('s', callback)
end

---注册进程退出信号回掉,注册此回掉后, 除非调用moon.quit, 服务不会马上退出。
---在回掉函数中可以处理异步逻辑（如带协程的数据库访问操作，收到退出信号后，保存数据）。
---注意：处理完成后必须要调用moon.quit,使服务自身退出,否则server进程将无法正常退出。
---@param callback fun()
function moon.exit(callback)
    assert(callback)
    core.set_cb('e', callback)
end

---注册服务对象销毁时的回掉函数，这个函数会在服务正常销毁时调用
---@param callback fun()
function moon.destroy(callback)
    core.set_cb('d', callback)
end

---@param msg core.message
---@param PTYPE string
local function _default_dispatch(msg, PTYPE)
    local p = protocol[PTYPE]
    if not p then
        error(string.format( "handle unknown PTYPE: %s. sender %u",PTYPE, msg:sender()))
    end

    local sessionid = msg:sessionid()
    if sessionid > 0 and PTYPE ~= PTYPE_ERROR then
        session_watcher[sessionid] = nil
        local co = session_id_coroutine[sessionid]
        if co then
            session_id_coroutine[sessionid] = nil
            --print(coroutine.status(co))
            coresume(co, p.unpack(msg))
            --print(coroutine.status(co))
            return
        end
        error(string.format( "%s: response [%u] can not find co.",moon.name(), sessionid))
	else
        if not p.dispatch then
			error(string.format( "[%s] dispatch PTYPE [%u] is nil",moon.name(), p.PTYPE))
			return
        end
        p.dispatch(msg, p)
    end
end

core.set_cb('m', _default_dispatch)

---
---向指定服务发送消息,消息内容会根据协议类型进行打包<br>
---param PTYPE:协议类型<br>
---param receiver:接收者服务id<br>
---param header:message header<br>
---param ...:消息内容<br>
---@param PTYPE string
---@param receiver int
---@param header string
---@return boolean
function moon.send(PTYPE, receiver, header, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end
    header = header or ''
    _send(sid_, receiver, p.pack(...), header, 0, p.PTYPE)
    return true
end

---向指定服务发送消息，消息内容不进行协议打包<br>
---param PTYPE:协议类型<br>
---param receiver:接收者服务id<br>
---param header:message header<br>
---param data 消息内容 string 类型<br>
---@param PTYPE string 协议类型
---@param receiver int 接收者服务id
---@param header string message header
---@param data string|userdata 消息内容
---@param sessionid int
---@return boolean
function moon.raw_send(PTYPE, receiver, header, data, sessionid)
	local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end
    header = header or ''
    sessionid = sessionid or 0
    _send(sid_, receiver, data, header, sessionid, p.PTYPE)
	return true
end

---获取当前的服务id
---@return int
function moon.sid()
    return sid_
end

---创建一个新的服务<br>
---param stype 服务类型，根据所注册的服务类型，可选有 'lua'<br>
---param config 服务的启动配置，数据类型table, 可以用来向服务传递初始化配置<br>
---param unique 是否是唯一服务，唯一服务可以用moon.queryservice(name) 查询服务id<br>
---param shared 可选，是否共享工作者线程，默认true<br>
---workerid 可选，工作者线程ID,在指定工作者线程创建该服务。默认0,服务将轮询加入工作者
---线程中<br>
---@param stype string
---@param config table
---@param unique boolean
---@param shared boolean
---@param workerid int
---@return bool
function moon.new_service(stype, config, unique, workerid)
    unique = unique or false
    workerid = workerid or 0
    config = jencode(config)
    return core.new_service(stype, config, unique, workerid,  0, 0)
end

---创建一个新的服务的协程封装，可以获得所创建的服务ID<br>
function moon.co_new_service(stype, config, unique, workerid)
    unique = unique or false
    workerid = workerid or 0
    config = jencode(config)
    local sessionid = make_response()
    local res = core.new_service(stype, config, unique, workerid, sid_, sessionid)
    if res then
        res = tonumber(co_yield())
    end
    return res
end

---异步移除指定的服务
---param sid 服务id
---param bresponse 可选，为true时可以 使用协程等待移除结果
---@param sid int
---@param bresponse boolean
---@return string
function moon.remove_service(sid, bresponse)
    if bresponse then
        local sessionid = make_response()
        core.remove_service(sid,sid_,sessionid,false)
        return co_yield()
    else
        core.remove_service(sid,0,0,false)
    end
end


---使当前服务退出
function moon.quit()
    moon.remove_service(sid_)
end

---根据服务name获取服务id,注意只能查询创建时配置unique=true的服务
--- 0 表示服务不存在
---@param name string
---@return int
function moon.queryservice(name)
	if type(name)=='string' then
		return core.queryservice(name)
	end
	return name
end

-------------------------协程操作封装--------------------------
local co_pool = setmetatable({}, {__mode = "kv"})

local waitallco = setmetatable({}, {__mode = "k"})

local function check_wait_all( ... )
    local co = co_running()
    local waitco = waitallco[co]
    if waitco then
        if 0 ~= waitco.ctx.count then
            waitco.ctx.results[waitco.idx] = {...}
            waitco.ctx.count= waitco.ctx.count-1
            if 0==waitco.ctx.count then
                coresume(waitco.ctx.cur,waitco.ctx.results)
            end
        end
    end
    return co
end

local function routine(f)
    while true do
        local co = check_wait_all(f())
        co_pool[#co_pool + 1] = co
        f = co_yield()
    end
end

---启动一个异步
---@param func function
---@return coroutine
function moon.async(func)
    local co = tremove(co_pool)
    if not co then
        co = co_create(routine)
    end
    coresume(co, func) --func 作为 routine 的参数
    return co
end

function moon.wait_all( ... )
    ---@type coroutine
    local currentco = co_running()
    local ctx = {count = select("#",...),results={},cur=currentco}
    for k=1,ctx.count do
        local co = select(k,...)
        assert(not waitallco[co])
        if type(co) == "thread" then
            ctx.results[k]=""
            waitallco[co]={ctx=ctx,idx=k}
        else
            ctx.results[k]=co
            ctx.count = ctx.count -1
        end
    end
    return co_yield()
end

function moon.coroutine_num()
    return #co_pool
end

--------------------------timer-------------
local timer_cb = {}

---@param mills int
---@param times int
---@param cb function
---@return int
function moon.repeated(mills, times, cb)
    local timerid = core.repeated(mills, times)
    timer_cb[timerid] = cb
    return timerid
end

core.set_cb('t',
    function(timerid, brm)
        if not brm then
            local cb = timer_cb[timerid]
            if cb then
                cb(timerid)
            end
        else
            timer_cb[timerid] = nil
        end
    end
)

local _repeated = moon.repeated

---@param mills int
---@return int
function moon.co_wait(mills)
    local co = co_running()
    _repeated(
        mills,
        1,
        function(tid)
            coresume(co, tid)
        end
    )
    return co_yield()
end
------------------------------------------

---@param serviceid int
---@return string
function moon.co_remove_service(serviceid)
    return moon.remove_service(serviceid, true)
end

---@param command string
---@return string
function moon.co_runcmd(command)
    local sessionid = make_response()
    core.runcmd(moon.sid(), command, sessionid)
    return co_yield()
end

---send-response 形式调用，发送消息附带一个responseid，对方收到后把responseid发送回来，
---必须调用moon.response应答.如果没有错误, 返回调用结果。如果发生错误第一个参数是false,
---后面是错误信息。
---param PTYPE:协议类型
---param receiver:接收者服务id
---param ...:发送的数据
---@param PTYPE string
---@param receiver int
---@return any
function moon.co_call(PTYPE, receiver, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon call unknown PTYPE[%s] message", PTYPE))
    end
    local sessionid = make_response(receiver)
	_send(sid_, receiver, p.pack(...), "", sessionid, p.PTYPE)
    return co_yield()
end

---回应moon.call
---param PTYPE 协议类型
---param receiver 调用者服务id
---param sessionid 调用者的responseid
---param ... 数据
---@param PTYPE string
---@param receiver int
---@param sessionid int
function moon.response(PTYPE, receiver, sessionid, ...)
    if sessionid == 0 then return end
    local p = protocol[PTYPE]
    if not p then
        error("handle unknown message")
    end
    _send(sid_, receiver, p.pack(...), '', sessionid, p.PTYPE)
end

------------------------------------
function moon.register_protocol(t)
    local PTYPE = t.PTYPE
    protocol[PTYPE] = t
    protocol[t.name] = t
end

local reg_protocol = moon.register_protocol


---设置指定协议消息的消息处理函数
---@param PTYPE string
---@param cb fun(msg:core.message,ptype:table)
---@return boolean
function moon.dispatch(PTYPE, cb)
    local p = protocol[PTYPE]
    if cb then
        local ret = p.dispatch
        p.dispatch = cb
        return ret
    else
        return p and p.dispatch
    end
end

--mark
local unpack = seri.unpack

reg_protocol {
    name = "lua",
    PTYPE = PTYPE_LUA,
    pack = seri.pack,
    unpack = function(msg)
        return unpack(msg:buffer())
    end,
    dispatch = function()
        error("PTYPE_LUA dispatch not implemented")
    end
}

reg_protocol {
    name = "text",
    PTYPE = PTYPE_TEXT,
    pack = function(...)
        return ...
    end,
    unpack = function(msg)
        return msg:bytes()
    end,
    dispatch = function()
        error("PTYPE_TEXT dispatch not implemented")
    end
}

reg_protocol {
    name = "error",
    PTYPE = PTYPE_ERROR,
    pack = function(...)
        return ...
    end,
    unpack = function(msg)
        return msg:bytes()
    end,
    dispatch = function(msg, p)
        local sessionid = msg:sessionid()
        local topic = msg:header()
        local data = p.unpack(msg)
        local co = session_id_coroutine[sessionid]
        if co then
            session_id_coroutine[sessionid] = nil
            coresume(co, false, topic..data)
            return
        end
    end
}

local system_command = {}

local ref_services = {}

--- mark a service, should not exit before this service
moon.retain =function(name)
    local id = moon.queryservice(name)
    moon.send("system", id, "retain", moon.name())
end

moon.release =function(name)
    local id = moon.queryservice(name)
    moon.send("system", id, "release", moon.name())
end

moon.co_wait_exit = function()
    while true do
        local num = 0
        for _,_ in pairs(ref_services) do
            num=num+1
        end
        if num ==0 then
            break
        else
            moon.co_wait(100)
        end
    end
    moon.quit()
end

system_command.exit = function(sender, msg)
    local data = msg:bytes()
    for k, v in pairs(session_watcher) do
        if v == sender then
            local co = session_id_coroutine[k]
            if co then
                session_id_coroutine[k] = nil
                coresume(co, false, data)
                return
            end
        end
    end
end

system_command.retain = function(_, msg)
    local data = msg:bytes()
    ref_services[data] = true
end

system_command.release = function(_, msg)
    local data = msg:bytes()
    ref_services[data] = nil
end

reg_protocol {
    name = "system",
    PTYPE = PTYPE_SYSTEM,
    pack = function(...)
        return ...
    end,
    unpack = function(msg)
        return msg:bytes()
    end,
    dispatch = function(msg, _)
        local sender = msg:sender()
        local header = msg:header()
        local func = system_command[header]
        if func then
            func(sender, msg)
        end
    end
}

reg_protocol{
    name = "socket",
    PTYPE = PTYPE_SOCKET,
    pack = function(...) return ... end,
    dispatch = function()
        error("PTYPE_SOCKET dispatch not implemented")
    end
}

reg_protocol{
    name = "websocket",
    PTYPE = PTYPE_SOCKET_WS,
    pack = function(...) return ... end,
    dispatch = function(_)
        error("PTYPE_SOCKET_WS dispatch not implemented")
    end
}

return moon
