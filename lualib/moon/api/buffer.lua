error("DO NOT REQUIRE THIS FILE")

--- Buffer's memory layout : head part + data part. Avoid memory copy when we write
--- data first and need write data's len at head part.

---@class buffer
local buffer = {}

--- Create a buffer, must use 'buffer.delete' free it.
---@param capacity? integer @ Data capacity, default 256
---@param headreserved? integer @ Head reserved size, default 14
---@return lightuserdata @buffer*
function buffer.unsafe_new(capacity, headreserved)

end

---@param buf lightuserdata @buffer*
function buffer.delete(buf)

end

---@param buf lightuserdata @buffer*
function buffer.clear(buf)

end

--- Get buffer's readable size
---@param buf lightuserdata @buffer*
---@return integer
function buffer.size(buf)

end

--- Get buffer's subytes or unpack subytes to integer
--- - buffer.unpack(buf, i, j)
--- - buffer.unpack(buf, fmt, i)
---@param buf lightuserdata @buffer*
---@param fmt? string| integer @ like string.unpack but only support '>','<','h','H','i','I'
---@param j integer @ offset
---@return string | any
function buffer.unpack(buf, fmt, j)
end

--- Read n bytes from buffer
---@param buf lightuserdata @buffer*
---@param n integer
---@return string
function buffer.read(buf, n)
end

--- Write string to buffer's head part
---@param buf lightuserdata @buffer*
---@param str string
function buffer.write_front(buf, str)
end

--- Write string to buffer
---@param buf lightuserdata @buffer*
---@param str string
function buffer.write_back(buf, str)
end

--- Seek buffer's read pos
---@param buf lightuserdata @buffer*
---@param pos integer
---@param origin? integer @ Seek's origin, Current:1, Begin:0, default 1
function buffer.seek(buf, pos, origin)
end

--- Offset buffer's write pos
---@param buf lightuserdata @buffer*
---@param n integer
function buffer.commit(buf, n)

end

--- Make buffer has enough space
---@param buf lightuserdata @buffer*
---@param n integer
function buffer.prepare(buf, n)

end

return buffer