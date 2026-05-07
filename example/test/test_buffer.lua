local moon = require("moon")
local buffer = require("buffer")
local seri = require("seri")
local test_assert = require("test_assert")

-- ============================================================
-- 1. unsafe_new / delete / size / clear
-- ============================================================
do
	local buf = buffer.unsafe_new()
	test_assert.equal(buffer.size(buf), 0)
	buffer.delete(buf)
end

do
	local buf = buffer.unsafe_new(4096)
	test_assert.equal(buffer.size(buf), 0)
	buffer.write_back(buf, "hello")
	test_assert.equal(buffer.size(buf), 5)
	buffer.clear(buf)
	test_assert.equal(buffer.size(buf), 0)
	buffer.delete(buf)
end

-- negative / zero capacity should fail
do
	local ok = pcall(buffer.unsafe_new, 0)
	test_assert.assert(not ok, "unsafe_new(0) should fail")

	ok = pcall(buffer.unsafe_new, -1)
	test_assert.assert(not ok, "unsafe_new(-1) should fail")

	ok = pcall(buffer.unsafe_new, 1024 * 1024 * 1024 + 1)
	test_assert.assert(not ok, "unsafe_new(capacity > 1GB) should fail")
end

-- ============================================================
-- 2. write_back - various types
-- ============================================================
do
	local buf = buffer.unsafe_new(256)

	-- string
	buffer.write_back(buf, "abc")
	test_assert.equal(buffer.unpack(buf), "abc")

	buffer.clear(buf)

	-- integer
	buffer.write_back(buf, 12345)
	test_assert.equal(buffer.unpack(buf), "12345")

	buffer.clear(buf)

	-- float
	buffer.write_back(buf, 3.14)
	local s = buffer.unpack(buf)
	test_assert.assert(s:find("3.14") ~= nil, "float write_back mismatch")

	buffer.clear(buf)

	-- boolean
	buffer.write_back(buf, true)
	test_assert.equal(buffer.unpack(buf), "true")

	buffer.clear(buf)

	buffer.write_back(buf, false)
	test_assert.equal(buffer.unpack(buf), "false")

	buffer.clear(buf)

	-- nil is ignored
	buffer.write_back(buf, nil)
	test_assert.equal(buffer.size(buf), 0)

	-- multiple args
	buffer.write_back(buf, "a", "b", "c")
	test_assert.equal(buffer.unpack(buf), "abc")

	buffer.clear(buf)

	-- table (array) recursive
	buffer.write_back(buf, {"x", "y", {"z"}})
	test_assert.equal(buffer.unpack(buf), "xyz")

	buffer.clear(buf)

	-- unsupported type should fail
	local ok = pcall(buffer.write_back, buf, function() end)
	test_assert.assert(not ok, "write_back function should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 3. unpack - string mode
-- ============================================================
do
	local buf = buffer.unsafe_new(128)
	buffer.write_back(buf, "ABCDEF")

	-- full
	test_assert.equal(buffer.unpack(buf), "ABCDEF")
	test_assert.equal(buffer.unpack(buf, 0), "ABCDEF")

	-- offset
	test_assert.equal(buffer.unpack(buf, 1), "BCDEF")
	test_assert.equal(buffer.unpack(buf, 5), "F")
	test_assert.equal(buffer.unpack(buf, 6), "")

	-- offset + count
	test_assert.equal(buffer.unpack(buf, 0, 3), "ABC")
	test_assert.equal(buffer.unpack(buf, 2, 2), "CD")

	-- count exceeding available data clamps
	test_assert.equal(buffer.unpack(buf, 4, 100), "EF")

	-- out of range position
	local ok = pcall(buffer.unpack, buf, 7)
	test_assert.assert(not ok, "unpack pos out of range should fail")

	ok = pcall(buffer.unpack, buf, -1)
	test_assert.assert(not ok, "unpack negative pos should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 4. unpack - binary mode (format string)
-- ============================================================
do
	local buf = buffer.unsafe_new(128)

	-- little-endian (default) int16
	buffer.write_back(buf, string.pack("<h", -1234))
	local v = buffer.unpack(buf, "h")
	test_assert.equal(v, -1234)

	buffer.clear(buf)

	-- big-endian uint16
	buffer.write_back(buf, string.pack(">H", 65535))
	v = buffer.unpack(buf, ">H")
	test_assert.equal(v, 65535)

	buffer.clear(buf)

	-- big-endian int32
	buffer.write_back(buf, string.pack(">i4", -100000))
	v = buffer.unpack(buf, ">i")
	test_assert.equal(v, -100000)

	buffer.clear(buf)

	-- big-endian uint32
	buffer.write_back(buf, string.pack(">I4", 3000000000))
	v = buffer.unpack(buf, ">I")
	test_assert.equal(v, 3000000000)

	buffer.clear(buf)

	-- combined format
	buffer.write_back(buf, string.pack(">HI4", 100, 200))
	local a, b = buffer.unpack(buf, ">HI")
	test_assert.equal(a, 100)
	test_assert.equal(b, 200)

	buffer.clear(buf)

	-- switch endian mid-format: big H then little i
	buffer.write_back(buf, string.pack(">H", 500))
	buffer.write_back(buf, string.pack("<i4", -999))
	a, b = buffer.unpack(buf, ">H<i")
	test_assert.equal(a, 500)
	test_assert.equal(b, -999)

	buffer.clear(buf)

	-- 'C' format returns pointer and remaining size
	buffer.write_back(buf, string.pack(">H", 42))
	buffer.write_back(buf, "PAYLOAD")
	local hdr, ptr, sz = buffer.unpack(buf, ">HC")
	test_assert.equal(hdr, 42)
	test_assert.equal(type(ptr), "userdata")
	test_assert.equal(sz, 7) -- "PAYLOAD" is 7 bytes

	buffer.clear(buf)

	-- unpack with pos offset
	buffer.write_back(buf, string.pack(">HI4", 111, 222))
	b = buffer.unpack(buf, ">I", 2)
	test_assert.equal(b, 222)

	local ok = pcall(buffer.unpack, buf, ">I", -1)
	test_assert.assert(not ok, "unpack format with negative pos should fail")

	buffer.clear(buf)

	-- invalid format character
	buffer.write_back(buf, "abcd")
	ok = pcall(buffer.unpack, buf, "X")
	test_assert.assert(not ok, "invalid format should fail")

	buffer.clear(buf)

	-- insufficient data
	buffer.write_back(buf, "ab") -- only 2 bytes, need 4 for 'i'
	ok = pcall(buffer.unpack, buf, "i")
	test_assert.assert(not ok, "insufficient data for unpack should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 5. read
-- ============================================================
do
	local buf = buffer.unsafe_new(128)
	buffer.write_back(buf, "HelloWorld")

	local s = buffer.read(buf, 5)
	test_assert.equal(s, "Hello")
	test_assert.equal(buffer.size(buf), 5)

	s = buffer.read(buf, 5)
	test_assert.equal(s, "World")
	test_assert.equal(buffer.size(buf), 0)

	-- read more than available
	buffer.write_back(buf, "short")
	local ok = pcall(buffer.read, buf, 100)
	test_assert.assert(not ok, "read beyond size should fail")

	ok = pcall(buffer.read, buf, -1)
	test_assert.assert(not ok, "read negative count should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 6. write_front
-- ============================================================
do
	local buf = buffer.unsafe_new(256)
	buffer.commit(buf, 8) -- reserve 8 bytes head space
	buffer.seek(buf, 8)

	buffer.write_front(buf, "HEADER")
	test_assert.equal(buffer.read(buf, 6), "HEADER")

	buffer.delete(buf)
end

do
	-- exceed prepend limit
	local buf = buffer.unsafe_new(256)
	buffer.commit(buf, 8)
	buffer.seek(buf, 8)

	local ok = pcall(buffer.write_front, buf, "123456789") -- 9 bytes > 8
	test_assert.assert(not ok, "write_front exceeding limit should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 7. seek
-- ============================================================
do
	local buf = buffer.unsafe_new(256)
	buffer.write_back(buf, "0123456789")

	-- seek current (default origin=1)
	buffer.seek(buf, 3)
	test_assert.equal(buffer.size(buf), 7)
	test_assert.equal(buffer.read(buf, 1), "3")

	-- seek from begin (origin=0 maps to Begin)
	-- after seek(3)+read(1), internal read pos is at offset 4
	-- seek(1, Begin) moves to absolute position 1, so next byte is "1"
	buffer.seek(buf, 1, 0)
	test_assert.equal(buffer.read(buf, 1), "1")

	-- seek out of range
	local ok = pcall(buffer.seek, buf, 100)
	test_assert.assert(not ok, "seek out of range should fail")

	ok = pcall(buffer.seek, buf, -1)
	test_assert.assert(not ok, "seek negative pos should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 8. prepare / commit
-- ============================================================
do
	local buf = buffer.unsafe_new(64)

	-- prepare(0) should fail
	local ok = pcall(buffer.prepare, buf, 0)
	test_assert.assert(not ok, "prepare(0) should fail")

	ok = pcall(buffer.prepare, buf, -1)
	test_assert.assert(not ok, "prepare(-1) should fail")

	buffer.prepare(buf, 100)
	-- commit more than prepared should fail
	ok = pcall(buffer.commit, buf, 10000)
	test_assert.assert(not ok, "commit exceeding prepared should fail")

	ok = pcall(buffer.commit, buf, -1)
	test_assert.assert(not ok, "commit negative size should fail")

	buffer.commit(buf, 10)
	test_assert.equal(buffer.size(buf), 10)

	buffer.delete(buf)
end

-- ============================================================
-- 9. concat
-- ============================================================
do
	-- basic concat
	local buf = buffer.concat("hello", " ", "world")
	test_assert.equal(buffer.unpack(buf), "hello world")
	buffer.delete(buf)
end

do
	-- concat with mixed types
	local buf = buffer.concat("n=", 42, " ok=", true)
	test_assert.equal(buffer.unpack(buf), "n=42 ok=true")
	buffer.delete(buf)
end

do
	-- concat empty returns nil (0 args)
	local result = buffer.concat()
	test_assert.equal(result, nil)
end

do
	-- concat with table
	local buf = buffer.concat({"a", "b"}, "c")
	test_assert.equal(buffer.unpack(buf), "abc")
	buffer.delete(buf)
end

do
	-- concat with raw pointer + len
	local src = buffer.unsafe_new(64)
	buffer.write_back(src, "raw")
	local ptr, sz = buffer.unpack(src, "C")
	local buf = buffer.concat("pre:", ptr, sz)
	test_assert.equal(buffer.unpack(buf), "pre:raw")
	buffer.delete(buf)
	buffer.delete(src)
end

-- ============================================================
-- 10. concat_string
-- ============================================================
do
	local s = buffer.concat_string("hello", " ", "world")
	test_assert.equal(s, "hello world")
end

do
	local s = buffer.concat_string("v=", 100, " f=", false)
	test_assert.equal(s, "v=100 f=false")
end

do
	-- empty
	local s = buffer.concat_string()
	test_assert.equal(s, "")
end

do
	-- table arg
	local s = buffer.concat_string({"x", "y"}, "z")
	test_assert.equal(s, "xyz")
end

do
	-- concat_string with raw pointer + len
	local src = buffer.unsafe_new(64)
	buffer.write_back(src, "tail")
	local ptr, sz = buffer.unpack(src, "C")
	local s = buffer.concat_string("head:", ptr, sz)
	test_assert.equal(s, "head:tail")
	buffer.delete(src)
end

-- ============================================================
-- 11. pack
-- ============================================================
do
	-- pack big-endian H I
	local buf = buffer.pack(">HI", 100, 200)
	local a, b = buffer.unpack(buf, ">HI")
	test_assert.equal(a, 100)
	test_assert.equal(b, 200)
	buffer.delete(buf)
end

do
	-- pack little-endian h i
	local buf = buffer.pack("<hi", -50, -100000)
	local a, b = buffer.unpack(buf, "<hi")
	test_assert.equal(a, -50)
	test_assert.equal(b, -100000)
	buffer.delete(buf)
end

do
	-- pack 'C' format (raw pointer + len)
	local data = "rawdata"
	local src = buffer.unsafe_new(64)
	buffer.write_back(src, data)
	local ptr, sz = buffer.unpack(src, "C")
	local buf = buffer.pack("C", ptr, sz)
	test_assert.equal(buffer.unpack(buf), data)
	buffer.delete(buf)
	buffer.delete(src)
end

do
	-- pack 'S' format can roundtrip via seri
	local payload = { name = "moon", ids = {1, 2, 3}, ok = true }
	local buf = buffer.pack("S", payload)
	local obj, _, remain = seri.unpack_one(buf, true)
	test_assert.equal(obj.name, payload.name)
	test_assert.linear_table_equal(obj.ids, payload.ids)
	test_assert.equal(obj.ok, payload.ok)
	test_assert.equal(remain, 0)
	buffer.delete(buf)
end

do
	-- pack with no args returns nil
	local result = buffer.pack()
	test_assert.equal(result, nil)
end

do
	-- pack invalid format
	local ok = pcall(buffer.pack, "Z", 1)
	test_assert.assert(not ok, "pack invalid format should fail")
end

do
	-- pack missing argument
	local ok = pcall(buffer.pack, "HH", 1) -- only 1 arg for 2 format chars
	test_assert.assert(not ok, "pack missing arg should fail")
end

do
	-- pack raw pointer with negative length should fail
	local src = buffer.unsafe_new(32)
	buffer.write_back(src, "bad")
	local ptr = select(1, buffer.unpack(src, "C"))
	local ok = pcall(buffer.pack, "C", ptr, -1)
	test_assert.assert(not ok, "pack raw pointer with negative length should fail")
	buffer.delete(src)
end

-- ============================================================
-- 12. to_shared
-- ============================================================
do
	local buf = buffer.unsafe_new(128)
	buffer.write_back(buf, "shared_data")
	local shared = buffer.to_shared(buf)
	-- shared is a userdata managed by GC
	test_assert.assert(shared ~= nil, "to_shared should return non-nil")
	test_assert.equal(type(shared), "userdata")

	-- can still unpack from shared
	local s = buffer.unpack(shared)
	test_assert.equal(s, "shared_data")
	test_assert.equal(buffer.size(shared), 11)
	-- no need to delete - GC handles it
end

do
	-- to_shared on empty buffer returns nil
	local buf = buffer.unsafe_new(128)
	local shared = buffer.to_shared(buf)
	test_assert.equal(shared, nil)
	buffer.delete(buf)
end

do
	-- to_shared with non-lightuserdata should fail
	local ok = pcall(buffer.to_shared, "not_a_buffer")
	test_assert.assert(not ok, "to_shared with string should fail")

	ok = pcall(buffer.to_shared, 123)
	test_assert.assert(not ok, "to_shared with number should fail")
end

-- ============================================================
-- 13. has_bitmask / add_bitmask
-- ============================================================
do
	local buf = buffer.unsafe_new(64)
	buffer.write_back(buf, "test")

	test_assert.equal(buffer.has_bitmask(buf, 1), false)
	test_assert.equal(buffer.has_bitmask(buf, 2), false)

	buffer.add_bitmask(buf, 1)
	test_assert.equal(buffer.has_bitmask(buf, 1), true)
	test_assert.equal(buffer.has_bitmask(buf, 2), false)

	buffer.add_bitmask(buf, 2)
	test_assert.equal(buffer.has_bitmask(buf, 1), true)
	test_assert.equal(buffer.has_bitmask(buf, 2), true)

	-- check combined mask
	buffer.add_bitmask(buf, 4)
	test_assert.equal(buffer.has_bitmask(buf, 4), true)

	buffer.delete(buf)
end

-- ============================================================
-- 14. append
-- ============================================================
do
	-- append single buffer
	local dst = buffer.unsafe_new(128)
	buffer.write_back(dst, "Hello")

	local src = buffer.unsafe_new(128)
	buffer.write_back(src, " World")

	buffer.append(dst, src)
	test_assert.equal(buffer.unpack(dst), "Hello World")
	test_assert.equal(buffer.size(dst), 11)

	-- source buffer is still valid
	test_assert.equal(buffer.unpack(src), " World")

	buffer.delete(dst)
	buffer.delete(src)
end

do
	-- append multiple buffers
	local dst = buffer.unsafe_new(128)
	buffer.write_back(dst, "A")

	local b1 = buffer.unsafe_new(64)
	buffer.write_back(b1, "B")
	local b2 = buffer.unsafe_new(64)
	buffer.write_back(b2, "C")
	local b3 = buffer.unsafe_new(64)
	buffer.write_back(b3, "D")

	buffer.append(dst, b1, b2, b3)
	test_assert.equal(buffer.unpack(dst), "ABCD")

	buffer.delete(dst)
	buffer.delete(b1)
	buffer.delete(b2)
	buffer.delete(b3)
end

do
	-- append accepts shared buffer userdata
	local dst = buffer.unsafe_new(64)
	local src = buffer.unsafe_new(64)
	buffer.write_back(src, "shared")
	local shared = buffer.to_shared(src)

	buffer.append(dst, shared)
	test_assert.equal(buffer.unpack(dst), "shared")

	buffer.delete(dst)
	shared = nil
end

do
	-- append rejects more than 64 source buffers
	local dst = buffer.unsafe_new(128)
	local sources = {}
	for i = 1, 65 do
		local src = buffer.unsafe_new(8)
		buffer.write_back(src, tostring(i))
		sources[i] = src
	end

	local ok = pcall(buffer.append, dst, table.unpack(sources))
	test_assert.assert(not ok, "append with more than 64 source buffers should fail")

	buffer.delete(dst)
	for i = 1, #sources do
		buffer.delete(sources[i])
	end
end

-- ============================================================
-- 15. mixed read/write stress
-- ============================================================
do
	local buf = buffer.unsafe_new(16) -- small initial capacity

	-- write enough to trigger realloc
	for i = 1, 100 do
		buffer.write_back(buf, "X")
	end
	test_assert.equal(buffer.size(buf), 100)

	-- read all
	local s = buffer.read(buf, 100)
	test_assert.equal(#s, 100)
	test_assert.equal(buffer.size(buf), 0)

	-- interleaved write/read
	for i = 1, 50 do
		buffer.write_back(buf, "ab")
		local r = buffer.read(buf, 2)
		test_assert.equal(r, "ab")
	end
	test_assert.equal(buffer.size(buf), 0)

	buffer.delete(buf)
end

-- ============================================================
-- 16. write_back with nested tables (depth limit)
-- ============================================================
do
	local buf = buffer.unsafe_new(256)

	-- deeply nested table should fail (MAX_DEPTH=32)
	local t = {"leaf"}
	for i = 1, 35 do
		t = {t}
	end
	local ok = pcall(buffer.write_back, buf, t)
	test_assert.assert(not ok, "deeply nested table should fail")

	buffer.delete(buf)
end

-- ============================================================
-- 17. pack/unpack roundtrip
-- ============================================================
do
	-- roundtrip: pack then unpack with same format
	local buf = buffer.pack(">HIhiH", 1000, 2000000, -100, -50000, 65535)
	local a, b, c, d, e = buffer.unpack(buf, ">HIhiH")
	test_assert.equal(a, 1000)
	test_assert.equal(b, 2000000)
	test_assert.equal(c, -100)
	test_assert.equal(d, -50000)
	test_assert.equal(e, 65535)
	buffer.delete(buf)
end

-- ============================================================
-- 18. clear then reuse
-- ============================================================
do
	local buf = buffer.unsafe_new(128)
	buffer.write_back(buf, "first")
	test_assert.equal(buffer.unpack(buf), "first")

	buffer.clear(buf)
	test_assert.equal(buffer.size(buf), 0)

	buffer.write_back(buf, "second")
	test_assert.equal(buffer.unpack(buf), "second")
	test_assert.equal(buffer.size(buf), 6)

	buffer.delete(buf)
end

-- ============================================================
-- Done
-- ============================================================

moon.async(function()
	test_assert.success()
end)
