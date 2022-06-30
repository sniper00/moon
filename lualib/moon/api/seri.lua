error("DO NOT REQUIRE THIS FILE")

---@class seri
local seri = {}

--- Pack lua objects to binary bytes, and return buffer* (avoid memory copy)
---@return buffer_ptr
---@nodiscard
function seri.pack(...)
end

--- Pack lua objects to binary bytes, and return lua string
---@return string
---@nodiscard
function seri.packs(...)
end

--- Unpack binary bytes to lua objects
---@param data string|cstring_ptr
---@param len? integer
---@return ...
---@nodiscard
function seri.unpack(data, len)

end

--- Unpack one lua object from binary bytes
---@param buf buffer_ptr
---@param isseek? boolean @ If true will seek buffer's read pos
---@return any, cstring_ptr, integer @return lua object, char*, charlen
function seri.unpack_one(buf, isseek)

end

--- Concat lua objects to string, and return buffer*
---@return buffer_ptr
function seri.concat(...)

end

--- Concat lua objects to string
---@return string
function seri.concats(...)

end

--- Concat lua objects to string, and return buffer*
---@param sep string
---@return buffer_ptr
function seri.sep_concat(sep, ...)

end

--- Concat lua objects to string
---@param sep string
---@return string
function seri.sep_concats(sep, ...)

end

return seri