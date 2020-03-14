local moon = require("moon")
local log = require("moon.log")
local json = require("json")
local seri = require("seri")
local fs = require("fs")
local test_assert = require("test_assert")

local equal = test_assert.equal

equal(type(moon.LOGV) ,"function")
equal(log.LOG_ERROR , 1)
equal(log.LOG_WARN ,2)
equal(log.LOG_INFO , 3)
equal(log.LOG_DEBUG , 4)

equal(type(checkbool) , "function")
equal(type(checkint) , "function")
equal(type(checknumber) , "function")
equal(type(checktable) , "function")

equal(type(class) , "function")
equal(type(iskindof) , "function")

equal(type(moon.name) , "function")
equal(type(moon.id) , "function")
equal(type(moon.send_prefab) , "function")
equal(type(moon.make_prefab) , "function")
equal(type(moon.broadcast) , "function")
equal(type(moon.queryservice) , "function")
equal(type(moon.new_service) , "function")
equal(type(moon.send) , "function")
equal(type(moon.set_cb) , "function")
equal(type(moon.abort) , "function")

local msg = moon.message
equal(type(msg.sender) , "function")
equal(type(msg.sessionid) , "function")
equal(type(msg.subtype) , "function")
equal(type(msg.header) , "function")
equal(type(msg.bytes) , "function")
equal(type(msg.size) , "function")
equal(type(msg.cstr) , "function")
equal(type(msg.buffer) , "function")

equal(type(fs.create_directory) , "function")
equal(type(fs.working_directory) , "function")
print(fs.working_directory())
equal(type(fs.exists) , "function")
equal(type(fs.traverse_folder) , "function")

local socket = require("asio")
equal(type(socket.listen) , "function")
equal(type(socket.accept) , "function")
equal(type(socket.connect) , "function")
equal(type(socket.read) , "function")
equal(type(socket.write) , "function")
equal(type(socket.write_with_flag) , "function")
equal(type(socket.write_message) , "function")
equal(type(socket.close) , "function")
equal(type(socket.settimeout) , "function")
equal(type(socket.setnodelay) , "function")
equal(type(socket.set_enable_frame) , "function")

equal(type(moon.millsecond) , "function")

local timer = moon
equal(type(timer.remove_timer) , "function")
equal(type(timer.repeated) , "function")

equal(type(moon.millsecond) , "function")
equal(type(string.hash) , "function")
equal(type(string.hex) , "function")
equal(type(table.new) , "function")

local nt = table.new(10,1)
nt[1] = 10

print("now server time")
moon.time_offset(1000*1000)--forward 1000s
print("after offset, now server time")

print("*********************api test ok**********************")

moon.make_prefab(nil)
moon.make_prefab("123")
moon.make_prefab(seri.pack("1",2,3,{a=1,b=2},nil))

moon.set_env("1","2")
equal(moon.get_env("1"),"2")
moon.set_env("1","3")
equal(moon.get_env("1"),"3")

local Base = class("Base")
function Base:ctor()
end

local Child = class("Child", Base)
function Child:ctor()
	Child.super.ctor(self)
end

local ChildB = class("ChildB", Base)
function ChildB:ctor()
	ChildB.super.ctor(self)
end

local c = Child.new()
local cb = ChildB.new()

equal(iskindof(c, "Child"),true)
equal(iskindof(c, "Base"),true)
equal(iskindof(c, "ChildB"),false)
equal(iskindof(cb, "ChildB"),true)

local tmr = moon

local ncount = 0
tmr.repeated(
	10,
	1,
	function()
		ncount = ncount + 1
		equal(ncount,1)
    end
)
local ntimes = 0
--example 一个超时10次的计时器 第二个参数是执行次数
tmr.repeated(
    10,
    10,
    function()
        ntimes = ntimes + 1
		test_assert.less_equal(ntimes,10)
    end
)

tmr.repeated(1,100,function()
	local t1 = tmr.repeated(1000,-1,function()
		test_assert.assert(false)
	end)
	tmr.remove_timer(t1)
end
)

moon.set_env("env_example","haha")
equal(moon.get_env("env_example"),"haha")

do
	local mt={}
	mt.names = "hahha"
	mt.__index = mt
	local tttt={
		a=1,
		b=2,
	}
	setmetatable(tttt,mt)
	local res = json.encode(tttt)
	test_assert.assert(res=='{"a":1,"b":2}' or res == '{"b":2,"a":1}')
end

do
	local buffer = require("buffer")
	local buf = buffer.unsafe_new(256)
	buffer.write_back(buf, "12345")
	assert(buffer.read(buf,5) == "12345")
	buffer.write_back(buf, "abcde")
	assert(buffer.read(buf,5) == "abcde")
	assert(buffer.size(buf) == 0)

	for i =1,1000 do
		buffer.write_back(buf,"abcde")
	end

	buffer.write_front(buf, "1000")

	assert(buffer.read(buf,4) == "1000")

	buffer.seek(buf,1)
	assert(buffer.read(buf,1) == "b")

	buffer.delete(buf)
end

moon.async(function()
	local co1 = moon.async(function()
		moon.sleep(100)
	end)

	moon.sleep(200)

	--coroutine reuse
	local co2 = moon.async(function()
		moon.sleep(100)
	end)

	-- create new coroutine
	local co3 = moon.async(function()
		moon.sleep(100)
	end)
	local running,free = moon.coroutine_num()
	test_assert.equal(free, 0)
	test_assert.equal(running, 3)

	moon.sleep(200)

	test_assert.equal(co1, co2)
	test_assert.assert(co1~=co3 and co2~=co3)
	running,free = moon.coroutine_num()
	test_assert.equal(free, 2)
	test_assert.equal(running, 1)


	for _= 1, 1000 do
		moon.async(function()
			moon.sleep(10)
		end)
	end

	moon.sleep(100)

	--coroutine reuse
	for _= 1, 1000 do
		moon.async(function()
			moon.sleep(10)
		end)
	end

	moon.sleep(100)

	running,free = moon.coroutine_num()
	test_assert.equal(free, 1000)

	test_assert.success()
end)

