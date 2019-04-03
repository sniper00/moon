require("base.io")
require("base.os")
require("base.string")
require("base.table")
require("base.util")
require("base.class")

---@class core
local core = require("moon.core")
local json = require("json")
local seri = require("seri")

local pairs = pairs
local json_encode = json.encode
local json_decode = json.decode
local co_create = coroutine.create
local co_running = coroutine.running
local _co_resume = coroutine.resume
local co_yield = coroutine.yield
local table_remove = table.remove
local table_insert = table.insert
local _send = core.send

local PTYPE_SYSTEM = 1
local PTYPE_TEXT = 2
local PTYPE_LUA = 3
local PTYPE_SOCKET = 4
local PTYPE_ERROR = 5
local PTYPE_SOCKET_WS = 6


---@class moon : core
local moon = {
    PTYPE_LUA = PTYPE_LUA
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

local response_uid = 1
local resplistener = {} --response message handler
local protocol = {}

local services_exited = {}
local response_wacther = {}

local function co_resume(co, ...)
    local ok, err = _co_resume(co, ...)
    if not ok then
        error(err)
    end
end

---make map<coroutine,responseid>
local function make_response(receiver)
    repeat
        response_uid = response_uid + 1
        if response_uid == 0xFFFFFFF then
            response_uid = 1
        end
    until not resplistener[response_uid]

    if receiver then
        if services_exited[receiver] then
            return false, string.format( "[%u] attempt send to dead service [%d]", sid_, receiver)
        end
        response_wacther[response_uid] = receiver
    end

    local co = co_running()
    resplistener[response_uid] = co
    return response_uid
end

moon.make_response = make_response

---
--- 注册服务初始化回掉函数,服务对象创建时会调用,可以在这里做服务自身的初始化操作
--- 回掉函数需要返回bool:true 初始化成功; false 服务始化失败.
---@param callback fun(config:table):boolean
function moon.init(callback)
    core.set_cb('i',function ( str )
        return callback(json_decode(str))
    end)
end

---
---注册服务启动回掉函数,这个函数的调用时机:
---1.如果服务是在配置文件中配置的服务，回掉函数会在所有配置的服务
---被创建之后调用。所以可以在回掉函数内查询其它唯一服务的 serviceid。
---2.运行时动态创建的服务，会在init之后，第一次update之前调用
---@param callback fun()
function moon.start(callback)
    core.set_cb('s', callback)
end

---注册server退出回掉,常用于带有异步流程的服务处理退出逻辑（如带协程的数据库服务，
---收到退出信号后，保存数据）。
---注意：处理完成后必须要调用moon.removeself，使服务自身退出,否则server将无法正常退出。
---@param callback fun()
function moon.exit(callback)
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

    local responseid = msg:responseid()
    if responseid > 0 and PTYPE ~= PTYPE_ERROR then
        response_wacther[responseid] = nil
        local co = resplistener[responseid]
        if co then
            resplistener[responseid] = nil
            --print(coroutine.status(co))
            co_resume(co, p.unpack(msg))
            --print(coroutine.status(co))
            return
        end
        error(string.format( "%s: response [%u] can not find co.",moon.name(), responseid))
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

    if services_exited[receiver] then
        return false,"send to a exited service"
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
---@param responseid int
---@return boolean
function moon.raw_send(PTYPE, receiver, header, data, responseid)
	local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end

    if services_exited[receiver] then
        return false,"moon.raw_send send to dead service"
	end
    header = header or ''
    responseid = responseid or 0
    _send(sid_, receiver, data, header, responseid, p.PTYPE)
	return true
end

---获取当前的服务id
---@return int
function moon.sid()
    return sid_
end

---创建一个新的服务<br>
---param stype 服务类型，根据所注册的服务类型，可选有 'lua'<br>
---param config 服务的启动配置，数据类型table, 可以用来向服务传递初始化配置(moon.init)<br>
---param unique 是否是唯一服务，唯一服务可以用moon.unique_service(name) 查询服务id<br>
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
    config = json_encode(config)
    return core.new_service(stype, config, unique, workerid,  0, 0)
end

---创建一个新的服务的协程封装，可以获得所创建的服务ID<br>
function moon.co_new_service(stype, config, unique, workerid)
    unique = unique or false
    workerid = workerid or 0
    config = json_encode(config)
    local responseid = make_response()
    local res = core.new_service(stype, config, unique, workerid, sid_, responseid)
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
        local rspid = make_response()
        core.remove_service(sid,sid_,rspid,false)
        return co_yield()
    else
        core.remove_service(sid,0,0,false)
    end
end


---使当前服务退出
function moon.removeself()
    moon.remove_service(sid_)
end

---根据服务name获取服务id,注意只能查询创建时配置unique=true的服务
---@param name string
---@return int
function moon.unique_service(name)
	if type(name)=='string' then
		return core.unique_service(name)
	end
	return name
end

---@param name string
---@param id int
function moon.set_unique_service(name,id)
	core.set_unique_service(name,id)
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
                co_resume(waitco.ctx.cur,waitco.ctx.results)
            end
        end
    end
    return co
end

local function _invoke(f, ...)
    return check_wait_all(f(...))
end

local function routine(f, ...)
    local co = check_wait_all(f(...))
    while true do
        table_insert(co_pool,co)
        --co_pool[#co_pool + 1] = co
        co = _invoke(co_yield())
    end
end

---启动一个异步
---@param func function
---@return coroutine
function moon.async(func, ...)
    local co = table_remove(co_pool)
    if not co then
        co = co_create(routine)
    end
    co_resume(co, func, ...) --func 作为 routine 的参数
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
            co_resume(co, tid)
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
    local rspid = make_response()
    core.runcmd(moon.sid(), command, rspid)
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

    local rspid,err = make_response(receiver)
    if not rspid then
        return false, err
    end

	_send(sid_, receiver, p.pack(...), "", rspid, p.PTYPE)
    return co_yield()
end

---回应moon.call
---param PTYPE 协议类型
---param receiver 调用者服务id
---param responseid 调用者的responseid
---param ... 数据
---@param PTYPE string
---@param receiver int
---@param responseid int
function moon.response(PTYPE, receiver, responseid, ...)
    if responseid == 0 then return end
    local p = protocol[PTYPE]
    if not p then
        error("handle unknown message")
    end
    _send(sid_, receiver, p.pack(...), '', responseid, p.PTYPE)
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
    unpack = function(arg)
        if arg.buffer then
            return unpack(arg:buffer())
        else
            return unpack(arg)
        end
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
    unpack = function(arg)
        if arg.bytes then
            return arg:bytes()
        else
            return arg
        end
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
    unpack = function(arg)
        if arg.bytes then
            return arg:bytes()
        else
            return arg
        end
    end,
    dispatch = function(msg, p)
        local responseid = msg:responseid()
        local topic = msg:header()
        local data = p.unpack(msg)
        --print("PTYPE_ERROR",topic,data)
        local co = resplistener[responseid]
        if co then
            co_resume(co, false, topic..data)
            resplistener[responseid] = nil
            return
        end
    end
}

local system_command = {}

local ref_services = {}

--- mark a service, should not exit before this service
moon.retain =function(name)
    local id = moon.unique_service(name)
    moon.send("system", id, "retain", moon.name())
end

moon.release =function(name)
    local id = moon.unique_service(name)
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
    moon.removeself()
end

system_command.exit = function(sender, msg)
    local data = msg:bytes()
    services_exited[sender] = true
    for k, v in pairs(response_wacther) do
        if v == sender then
            local co = resplistener[k]
            if co then
                co_resume(co, false, data)
                resplistener[k] = nil
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
    unpack = function(arg)
        if arg.bytes then
            return arg:bytes()
        else
            return arg
        end
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
