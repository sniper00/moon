local moon = require("moon")
local seri = require("seri")
local buffer = require("buffer")
local test_assert = require("test_assert")

do
	moon.dispatch("text", function(msg)
		assert(moon.decode(msg, "Z") == "123")
	end, true)
	local perfabid = moon.make_prefab("123")
	moon.raw_send("text", moon.id, "", perfabid, 0)
	moon.raw_send("text", moon.id, "", perfabid, 0)
	moon.raw_send("text", moon.id, "", perfabid, 0)
	moon.raw_send("text", moon.id, "", perfabid, 0)

	assert(moon.make_prefab("123"))
	assert(moon.make_prefab(seri.pack("1", 2, 3, { a = 1, b = 2 }, nil)))

	moon.env("1", "2")
	assert(moon.env("1") == "2")
	moon.env("1", "3")
	assert(moon.env("1") == "3")

	print(moon.server_stats())

	moon.loglevel("DEBUG")
	assert(moon.loglevel() == 4)
end

do
	do
		local data = seri.packs(1,"hello", {a=1,b=2})
		assert(type(data) == "string")
		local v1,v2,v3 = seri.unpack(data)
		assert(v1 == 1)
		assert(v2 == "hello")
		assert(v3.a == 1 and v3.b == 2)
	end

	do
		local pbuffer = seri.pack(1,"hello", {a=1,b=2})
		local sz, len = buffer.unpack(pbuffer, 'C')
		local v1,v2,v3 = seri.unpack(sz, len)
		assert(v1 == 1)
		assert(v2 == "hello")
		assert(v3.a == 1 and v3.b == 2)

		assert(seri.unpack_one(pbuffer) == 1)
		assert(seri.unpack_one(pbuffer) == 1)
		assert(seri.unpack_one(pbuffer, true) == 1)
		assert(seri.unpack_one(pbuffer, true) == "hello")

		buffer.delete(pbuffer)
	end

	do
		local pbuffer = seri.concat(1,2,3,4)
		assert(buffer.unpack(pbuffer) == "1234")
		buffer.delete(pbuffer)
	end

	do
		local pbuffer = seri.sep_concat(',', 1,2,3,4)
		assert(buffer.unpack(pbuffer) == "1,2,3,4")
		buffer.delete(pbuffer)
	end
end

do
	local buffer = require("buffer")

	do
		local buf = buffer.unsafe_new(128)
		buffer.write_back(buf, "1234")
		assert(buffer.unpack(buf) == "1234")
		assert(buffer.unpack(buf, 0) == "1234")
		assert(buffer.unpack(buf, 1) == "234")
		assert(buffer.unpack(buf, 1, 1) == "2")
		assert(buffer.unpack(buf, 1, 2) == "23")
		assert(buffer.unpack(buf, 1, 3) == "234")
		assert(buffer.unpack(buf, 2, 3) == "34")
		assert(buffer.unpack(buf, 2, 100) == "34")
		assert(buffer.unpack(buf, 4) == "")
		local ok, err = pcall(buffer.unpack, buf, 5)
		assert(not ok)
		buffer.delete(buf)
	end

	do
		local buf = buffer.unsafe_new(128)
		buffer.write_back(buf, string.pack(">HI", 100, 200))
		assert(buffer.unpack(buf, ">H") == 100)
		assert(buffer.unpack(buf, ">I", 2) == 200)
		local a, b = buffer.unpack(buf, ">HI")
		assert(a == 100 and b == 200)

		local ok, err = pcall(buffer.unpack, buf, "H", 5)
		assert(not ok)

		local ok, err = pcall(buffer.unpack, buf, "H", 6)
		assert(not ok)

		local a, p, n = buffer.unpack(buf, ">HC")
		assert(type(p) == "userdata")
		assert(type(n) == "number")
		assert(n == 4)

		local a, b, p, n = buffer.unpack(buf, ">HIC")
		assert(n == 0)
		buffer.delete(buf)
	end

	do
		local buf = buffer.unsafe_new(256)

		buffer.write_back(buf, "1234")
		local ok, err = pcall(buffer.read, buf, 5)
		assert(not ok)
		assert(buffer.read(buf, 4))
		buffer.delete(buf)
	end


	do
		local buf = buffer.unsafe_new(256, 8)
		buffer.write_back(buf, "12345")
		assert(buffer.read(buf, 5) == "12345")
		buffer.write_back(buf, "abcde")
		assert(buffer.read(buf, 5) == "abcde")
		assert(buffer.size(buf) == 0)

		for i = 1, 1000 do
			buffer.write_back(buf, "abcde")
		end

		assert(not buffer.write_front(buf, "123456789"))

		buffer.write_front(buf, "12345678")

		assert(buffer.read(buf, 8) == "12345678")

		buffer.seek(buf, 1)
		assert(buffer.read(buf, 1) == "b")

		buffer.delete(buf)
	end

	do
		local buf = buffer.unsafe_new(256)
		buffer.write_back(buf, "12345")
		assert(buffer.read(buf, 5) == "12345")
		buffer.write_back(buf, "abcde")
		assert(buffer.read(buf, 5) == "abcde")
		assert(buffer.size(buf) == 0)

		for i = 1, 1000 do
			buffer.write_back(buf, "abcde")
		end

		buffer.write_front(buf, "1000")

		assert(buffer.read(buf, 4) == "1000")

		buffer.seek(buf, 1)
		assert(buffer.read(buf, 1) == "b")

		buffer.delete(buf)
	end
end

do
	local list = require("list")

	local q = list.new()
	list.push(q, 4)
	assert(list.size(q) == 1)
	list.push(q, 3)
	assert(list.size(q) == 2)
	list.push(q, 2)
	assert(list.size(q) == 3)
	list.push(q, 1)
	assert(list.size(q) == 4)
	assert(list.pop(q) == 4)
	assert(list.size(q) == 3)
	assert(list.pop(q) == 3)
	assert(list.size(q) == 2)
	assert(list.pop(q) == 2)
	assert(list.size(q) == 1)
	assert(list.pop(q) == 1)
	assert(list.size(q) == 0)
end

do
	local tablex = require("tablex")
	assert(tablex.remove({ 100, 200, 300 }, 100) == 100)
	local t = { 100, 200, 300 }
	tablex.remove(t, 100)
	table.equals(t, { 300, 200 })
	assert(tablex.remove({ 100 }, 100) == 100)
	t = { 100 }
	tablex.remove(t, 100)
	table.equals(t, {})
end

do
	local datetime = require("moon.datetime")

	do
		local t1 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 11,
			hour = 23,
			min = 59,
			sec = 59
		})

		local t2 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 14,
			hour = 23,
			min = 59,
			sec = 59
		})

		local t3 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 15,
			hour = 0,
			min = 0,
			sec = 0
		})

		assert(datetime.is_same_week(t1, t2))
		assert(not datetime.is_same_week(t1, t3))
		assert(not datetime.is_same_week(t2, t3))
	end

	do
		local year, yweek, weekday = datetime.isocalendar(datetime.make_time({
			year = 2021,
			month = 1,
			day = 1,
			hour = 0,
			min = 0,
			sec = 0
		}))

		assert(year == 2020)
		assert(yweek == 53)
		assert(weekday == 5)

		year, yweek, weekday = datetime.isocalendar(datetime.make_time({
			year = 2021,
			month = 1,
			day = 4,
			hour = 0,
			min = 0,
			sec = 0
		}))
		assert(year == 2021)
		assert(yweek == 1)
		assert(weekday == 1)
	end

	do
		local t1 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 12,
			hour = 0,
			min = 0,
			sec = 0
		})

		local t2 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 13,
			hour = 0,
			min = 0,
			sec = 0
		})

		local t3 = datetime.make_time({
			year = 2021,
			month = 11,
			day = 13,
			hour = 23,
			min = 59,
			sec = 59
		})

		assert(not datetime.is_same_day(t1, t2))
		assert(datetime.is_same_day(t2, t3))

		assert(datetime.is_same_month(t2, t3))
	end

	do
		local t = datetime.make_hourly_time(moon.time(), 0)
		local tm = datetime.localtime(t)
		assert(tm.hour == 0)
		assert(tm.min == 0)
		assert(tm.sec == 0)

		t = datetime.make_hourly_time(moon.time())
		tm = datetime.localtime(t)
		assert(tm.hour == 12)
		assert(tm.min == 0)
		assert(tm.sec == 0)
	end
end

do
	moon.async(function()
		local ncount = 0
		moon.timeout(
			0,
			function()
				ncount = ncount + 1
				test_assert.equal(ncount, 1)
			end
		)

		local co1 = moon.async(function()
			moon.sleep(0)
		end)

		local co2 = moon.async(function()
			moon.sleep(0)
		end)

		-- create new coroutine
		local co3 = moon.async(function()
			moon.sleep(0)
		end)
		local running, free = moon.coroutine_num()
		--print(running, free)
		test_assert.equal(free, 0)
		test_assert.equal(running, 4)

		moon.sleep(100)

		running, free = moon.coroutine_num()
		test_assert.equal(free, 3)
		test_assert.equal(running, 1)

		for _ = 1, 1000 do
			moon.async(function()
				moon.sleep(0)
			end)
		end

		moon.sleep(100)

		--coroutine reuse
		for _ = 1, 1000 do
			moon.async(function()
				moon.sleep(0)
			end)
		end

		moon.sleep(100)

		running, free = moon.coroutine_num()
		test_assert.equal(free, 1000)

		test_assert.success()
	end)
end
