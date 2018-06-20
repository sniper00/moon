local core = require("moon_core")
local json = require("json")
local seri = require("seri")
require("log") -- hook print

local json_encode = json.encode
local json_decode = json.decode
local co_create = coroutine.create
local co_running = coroutine.running
local _co_resume = coroutine.resume
local co_yield = coroutine.yield
local table_remove = table.remove

local PTYPE_SYSTEM = 1
local PTYPE_TEXT = 2
local PTYPE_LUA = 3
local PTYPE_SOCKET = 4
local PTYPE_ERROR = 5

local moon = {
    PSYSTEM = PTYPE_SYSTEM,
    PTEXT = PTYPE_TEXT,
    PLUA = PTYPE_LUA,
    PSOCKET = PTYPE_SOCKET,
    PERROR = PTYPE_ERROR
}

setmetatable(moon, {__index = core})

-- export global variable
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
        __newindex = function(_, name)
            local msg = string.format('USE "moon.exports.%s = <value>" INSTEAD OF SET GLOBAL VARIABLE', name)
            print(debug.traceback(msg, 2))
            print("")
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

local waitallco = {}

local function co_resume(co, ...)
    local ok, err = _co_resume(co, ...)
    if not ok then
        error(err)
    end
end

-- map co with uid
local function make_response(receiver)
    repeat
        response_uid = response_uid + 1
        if response_uid == 0xFFFFFFF then
            response_uid = 1
        end
    until not resplistener[response_uid]

    if receiver then
        if services_exited[receiver] then
            return false
        end
        response_wacther[response_uid] = receiver
    end

    local co = co_running()
    resplistener[response_uid] = co
    return response_uid
end

moon.make_response = make_response


--[[
	注册服务初始化回掉函数,服务被创建时会调用回掉函数
	@param callback 函数签名 function(config:table) return bool end ;config 服务的启动配置，参见config.json.
	注意 回掉函数返回true：初始化成功. false 服务创建失败.
]]
function moon.init(callback)
    core.set_init(function ( str )
        return callback(json_decode(str))
    end)
end

--[[
	注册服务启动回掉函数，这个函数会在init之后调用
	@param callback 函数签名 function() end
]]
function moon.start(callback)
    core.set_start(callback)
end

--[[
    注册server退出回掉,必须在回掉函数中 调用moon.removeself ,否则server退出流程将阻塞。
    常用于带有异步流程的服务（如带协程的数据库服务），收到退出信号后，处理数据保存任务，然后removeself
	@param callback 函数签名 function() end
]]
function moon.exit(callback)
    core.set_exit(callback)
end

--[[
    注册服务销毁回掉函数，这个函数会在服务正常销毁时调用
	@param callback: function() end
]]
function moon.destroy(callback)
    core.set_destroy(callback)
end

-- default handle
local function _default_dispatch(msg, PTYPE)
    local p = protocol[PTYPE]
    if not p then
        error(string.format( "handle unknown PTYPE: %s",PTYPE))
    end

    local responseid = msg:responseid()

    if responseid > 0 and PTYPE ~= PTYPE_ERROR then
        response_wacther[responseid] = nil
        local co = resplistener[responseid]
        if co then
            --print(coroutine.status(co))
            co_resume(co, p.unpack(msg))
            resplistener[responseid] = nil
            return
        end
        error(string.format( "response [%u] can not find co", responseid))
	else
        if not p.dispatch then
			error(string.format( "[%s] dispatch null [%u]",moon.name(), p.PTYPE))
			return
        end
        p.dispatch(msg, p)
    end
end

core.set_dispatch(_default_dispatch)

--[[
    向指定服务发送消息,消息内容会根据协议类型进行打包
	@param PTYPE:协议类型
	@param receiver:接收者服务id
	@param header:message header
	@param ...:消息内容,注意不能发送函数类型
]]
function moon.send(PTYPE, receiver, header, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end

    if services_exited[receiver] then
        return false,"send to a exited service"
    end
    header = header or ''
    return core.send(sid_, receiver, p.pack(...), header, 0, p.PTYPE)
end

--[[
    向指定服务发送消息，消息内容不进行协议打包
	@param PTYPE:协议类型
	@param receiver:接收者服务id
	@param header:message header
    @param data 消息内容 string 类型
]]
function moon.raw_send(PTYPE, receiver, header, data)
	local p = protocol[PTYPE]
    if not p then
        error(string.format("moon send unknown PTYPE[%s] message", PTYPE))
    end

    if services_exited[receiver] then
        print("moon.raw_send send to a crashed service")
        return false
	end
    header = header or ''
	core.send(sid_, receiver, data, header, 0, p.PTYPE)
end

function moon.co_call_with_header(PTYPE, receiver, header, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon call unknown PTYPE[%s] message", PTYPE))
    end

    if services_exited[receiver] then
        return false, "call a exited service"
	end

    local responseid = make_response()
    response_wacther[responseid] = receiver
    header = header or ''
	core.send(sid_, receiver, p.pack(...), header, responseid, p.PTYPE)
    return co_yield()
end

--[[
    获取当前的服务id
	@return int
]]
function moon.sid()
    return sid_
end

--[[
	创建一个新的服务
	@param stype 服务类型，根据所注册的服务类型，可选有 'lua'
    @param config 服务的启动配置，数据类型table, 可以用来向服务传递初始化配置(moon.init)
    @param unique 是否是唯一服务，唯一服务可以用moon.unique_service(name) 查询服务id
	@param shared 可选，是否共享工作者线程，默认true
	@workerid 可选，工作者线程ID,在指定工作者线程创建该服务。默认0,服务将轮询加入工作者线程中
	@return int 返回所创建的服务ID, 0代表创建失败
]]
function moon.new_service(stype, config, unique, shared, workerid)
    unique = unique or false
    shared = shared or true
    workerid = workerid or 0
    config = json_encode(config)
    return core.new_service(stype, unique, shared, workerid, config)
end

--[[
	异步移除指定的服务
	@param 服务id
	@param 可选，为true时可以 使用协程等待移除结果
]]
function moon.remove_service(sid, bresponse)
    local header = "rmservice." .. sid
    if bresponse then
        local rspid = make_response()
        core.runcmd(moon.sid(), "", header, rspid)
        return co_yield()
    else
        core.runcmd(0, "", header, 0)
    end
end

--[[
	使当前服务退出
]]
function moon.removeself()
    moon.remove_service(sid_)
end

function moon.query_worktime(workerid, bresponse)
    local header = "workertime." .. workerid

    if bresponse then
        local rspid = make_response()
        core.runcmd(moon.sid(), "", header, rspid)
        return co_yield()
    else
        core.runcmd(0, "", header, 0)
    end
end

--[[
	根据服务name获取服务id,注意只能查询创建时unique配置为true的服务
]]
function moon.unique_service(name)
	if type(name)=='string' then
		return core.unique_service(name)
	end
	return name
end

function moon.set_unique_service(name,id)
	core.set_unique_service(name,id)
end

-------------------------协程操作封装--------------------------
local co_pool = setmetatable({}, {__mode = "kv"})

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

local function routine(func)
    while true do
        local co = check_wait_all(func())
        co_pool[#co_pool + 1] = co
        func = co_yield()
    end
end

function moon.start_coroutine(func)
    local co = table_remove(co_pool)
    if not co then
        co = co_create(routine)
    end
    co_resume(co, func) --func 作为 routine 的参数
    return co
end

function moon.wait_all( ... )
    local currentco = co_running()
    local cos = {...}
    local ctx = {count = #cos,results={},cur=currentco}
    for k,co in pairs(cos) do
        --assert(not waitallco[co])
        ctx.results[k]=""
        waitallco[co]={ctx=ctx,idx=k}
    end
    return co_yield()
end

--------------------------timer-------------
local timer_cb = {}

function moon.repeated(mills, times, cb)
    local timerid = core.repeated(mills, times)
    timer_cb[timerid] = cb
    return timerid
end

core.set_on_timer(
    function(timerid)
        local cb = timer_cb[timerid]
        if cb then
            cb(timerid)
        end
    end
)

core.set_remove_timer(
    function(timerid)
        timer_cb[timerid] = nil
    end
)

local _repeated = moon.repeated

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

function moon.co_remove_service(serviceid)
    local co = co_running()
    moon.remove_service(serviceid, co)
    return co_yield()
end

function moon.co_query_worktime(workerid)
    local co = co_running()
    moon.query_worktime(workerid, co)
    return co_yield()
end

--[[
	send-response 形式调用，发送消息附带一个responseid，对方收到后把
	responseid发送回来，必须调用moon.response应答.

	@param PTYPE:协议类型
	@param receiver:接收者服务id
	@param ...:发送的数据
	如果没有错误, 返回调用结果。如果发生错误第一个参数是false, 后面是错误信息
]]
function moon.co_call(PTYPE, receiver, ...)
    local p = protocol[PTYPE]
    if not p then
        error(string.format("moon call unknown PTYPE[%s] message", PTYPE))
    end

    local rspid = make_response(receiver)
    if not rspid then
        return false, "call a exited service"
    end

	core.send(sid_, receiver, p.pack(...), "", rspid, p.PTYPE)
    return co_yield()
end

--[[
	回应moon.call
	@param PTYPE:协议类型
	@param receiver:调用者服务id
	@param responseid:调用者的responseid
	@param ...:数据
]]
function moon.response(PTYPE, receiver, responseid, ...)
    local p = protocol[PTYPE]
    if not p then
        error("handle unknown message")
    end
    core.send(sid_, receiver, p.pack(...), '', responseid, p.PTYPE)
end

------------------------------------
function moon.register_protocol(t)
    local PTYPE = t.PTYPE
    protocol[PTYPE] = t
    protocol[t.name] = t
end

local reg_protocol = moon.register_protocol

--[[
    给指定协议消息，设置消息处理函数
    cb 函数签名 function(msg:message,p:table)
]]
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
moon.pack = seri.pack
moon.unpack = seri.unpack

reg_protocol {
    name = "lua",
    PTYPE = PTYPE_LUA,
    pack = seri.pack,
    unpack = function(arg)
        if type(arg) == "string" then
            return seri.unpack(arg)
        else -- message
            return seri.unpack(arg:buffer())
        end
    end
}

reg_protocol {
    name = "text",
    PTYPE = PTYPE_TEXT,
    pack = function(...)
        return ...
    end,
    unpack = function(arg)
        if type(arg) == "string" then
            return arg
        else -- message
            return arg:bytes()
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
        if type(arg) == "string" then
            return arg
        else -- message
            return arg:bytes()
        end
    end,
    dispatch = function(msg, p)
        local responseid = msg:responseid()
        local topic = msg:header()
        local data = p.unpack(msg)
        --print("moon.PTYPE_ERROR",topic,data)
        local co = resplistener[responseid]
        if co then
            co_resume(co, false, topic, data)
            resplistener[responseid] = nil
            return
        end
    end
}

reg_protocol {
    name = "system",
    PTYPE = PTYPE_SYSTEM,
    pack = function(...)
        return ...
    end,
    unpack = function(arg)
        if type(arg) == "string" then
            return arg
        else -- message
            return arg:bytes()
        end
    end,
    dispatch = function(msg, _)
        local sender = msg:sender()
        local header = msg:header()
        if header == "exit" then
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
        return true
    end
}

return moon
