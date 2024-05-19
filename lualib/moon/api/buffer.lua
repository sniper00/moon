---@meta
error("DO NOT REQUIRE THIS FILE")
--- Represents a `Cpp Buffer` object, which is not managed by `Lua GC`. 
--- This is often used for data transmission between Lua and Cpp layers.
--- This object can be used as an argument for `moon.raw_send` or `socket.write`, 
--- and it will be automatically released. Otherwise, `buffer.drop` should be used to release it.
---@class buffer
local buffer = {}

--- Creates a `Cpp Buffer` object that is not managed by `Lua GC`. 
--- This object can be used as an argument for `moon.raw_send` or `socket.write`, 
--- and it will be automatically released. Otherwise, `buffer.drop` should be used to release it.
---@param capacity? integer @ The initial capacity of the Buffer, default value is `128`.
---@return buffer_ptr
function buffer.unsafe_new(capacity) end

--- Releases a `Cpp Buffer` object.
---@param buf buffer_ptr
function buffer.delete(buf) end

--- Clears the data in the buffer.
---@param buf buffer_ptr
function buffer.clear(buf) end

--- Get buffer's readable size
---@param buf buffer_ptr
---@return integer
function buffer.size(buf) end

--- buffer.unpack(buf, pos, count) returns a portion of the buffer data. 
--- The optional parameter `pos` (default is 0) marks where to start reading from the buffer, 
--- and `count` indicates how much data to read.
---
--- buffer.unpack(buf, fmt, pos) unpacks the buffer data according to the `fmt` format. 
--- The optional parameter `pos` (default is 0) marks where to start reading from the buffer.
---
--- @param buf buffer_ptr|buffer_shr_ptr
--- @param fmt? string @ like string.unpack but only supports '>', '<', 'h', 'H', 'i', 'I'
--- @param pos? integer @ start position
--- @param count? integer @ number of elements to read
--- @return string | any
--- @overload fun(buf:buffer_ptr, pos:integer, count?:integer)
function buffer.unpack(buf, fmt, pos, count) end

--- Read n bytes from buffer
---@param buf buffer_ptr
---@param n integer
---@return string
function buffer.read(buf, n) end

--- Write string to buffer's head part
---@param buf buffer_ptr
---@param ... string
function buffer.write_front(buf, ...) end

--- Write string to buffer
---@param buf buffer_ptr
---@param ... string
function buffer.write_back(buf, ...) end

--- Moves the read position of the buffer
---@param buf buffer_ptr
---@param pos integer
---@param origin? integer @ Seek's origin, Current:1, Begin:0, default 1
function buffer.seek(buf, pos, origin) end

--- Moves the write position of the buffer forward.
---@param buf buffer_ptr
---@param n integer
function buffer.commit(buf, n) end

--- Ensures that the buffer can accommodate n characters,reallocating character array objects as necessary.
---@param buf buffer_ptr
---@param n integer
function buffer.prepare(buf, n) end

--- Converts the parameters to a string and saves it in the buffer, 
--- then returns a lightuserdata. This is often used for data transmission between Lua and Cpp layers, 
--- to avoid creating Lua GC objects.
---@return buffer_ptr
function buffer.concat(...) end

--- Converts the parameters to a string.
---@return string
function buffer.concat_string(...) end

--- Converts a buffer_ptr into userdata(buffer_shr_ptr)
---@param buf buffer_ptr
---@return buffer_shr_ptr
function buffer.to_shared(buf) end

---@param buf buffer_ptr
---@param mask integer
---@return boolean
function buffer.has_bitmask(buf, mask) end

---@param buf buffer_ptr
---@param mask integer
function buffer.add_bitmask(buf, mask) end

return buffer