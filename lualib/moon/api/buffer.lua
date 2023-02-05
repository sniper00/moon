---@meta
error("DO NOT REQUIRE THIS FILE")

--- Buffer's memory layout : head part + data part. Avoid memory copy when we write
--- data first and need write data's length at head part.

---@class buffer
local buffer = {}

---创建一个不受`Lua GC`管理的`C++ Buffer`对象, 可以作为`moon.raw_send`或者`socket.write`的参数, 会自动管理该对象的生命周期。否则应该使用`buffer.delete`释放它。
---@param capacity? integer @ Buffer的初始容量, 默认值 `240`。
---@param headreserved? integer @ Buffer头部的预留空间, 默认值 `16`。
---@return buffer_ptr
function buffer.unsafe_new(capacity, headreserved) end

---@param buf buffer_ptr
function buffer.delete(buf) end

---@param buf buffer_ptr
function buffer.clear(buf) end

--- Get buffer's readable size
---@param buf buffer_ptr
---@return integer
function buffer.size(buf) end

--- Get buffer's subytes or unpack subytes to integer
--- - buffer.unpack(buf, i, count)
--- - buffer.unpack(buf, fmt, i)
---@param buf buffer_ptr
---@param fmt? string @ like string.unpack but only support '>','<','h','H','i','I'
---@param i? integer @ start pos
---@param count? integer @
---@return string | any
---@overload fun(buf:buffer_ptr, i:integer, count?:integer)
function buffer.unpack(buf, fmt, i, count) end

--- Read n bytes from buffer
---@param buf buffer_ptr
---@param n integer
---@return string
function buffer.read(buf, n) end

--- Write string to buffer's head part
---@param buf buffer_ptr
---@param str string
function buffer.write_front(buf, str) end

--- Write string to buffer
---@param buf buffer_ptr
---@param str string
function buffer.write_back(buf, str) end

--- Seek buffer's read pos
---@param buf buffer_ptr
---@param pos integer
---@param origin? integer @ Seek's origin, Current:1, Begin:0, default 1
function buffer.seek(buf, pos, origin) end

--- Offset buffer's write pos
---@param buf buffer_ptr
---@param n integer
function buffer.commit(buf, n) end

---Ensures that the buffer can accommodate n characters,reallocating character array objects as necessary.
---@param buf buffer_ptr
---@param n integer
function buffer.prepare(buf, n) end

---@param buf buffer_ptr
---@param k integer
---@return boolean
function buffer.has_flag(buf, k) end

---@param buf buffer_ptr
---@param k integer
function buffer.set_flag(buf, k) end

--- All params are strings or numbers and return buffer_ptr(lightuserdata type, avoid creating Lua GC objects): `param1..param2 ··· ..paramN`. 
---@return buffer_ptr
function buffer.concat(...) end

--- All params are strings or numbers and return string: `param1..param2 ··· ..paramN`. 
---@return string
function buffer.concat_string(...) end

return buffer