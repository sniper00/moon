---@meta

error("DO NOT REQUIRE THIS FILE")

---@class json
---@field null lightuserdata @ Represents json "null" values
local json = {}

---@param t table|number|string|boolean
---@param empty_as_array? boolean @default true
---@param format? boolean @default true, pretty
---@return string
function json.encode(t, empty_as_array, format)
end

---@param t table
---@return string
function json.pretty_encode(t) end

---@param str string|cstring_ptr
---@param n? integer
---@return table
function json.decode(str, n) end

--- Concat array item to string, table type will encode to json string
---@param array any[]
---@return buffer_ptr
---@overload fun(str:string):buffer_ptr
function json.concat(array) end

--- concat as redis protocol
---@return buffer_ptr
function json.concat_resp(...) end

return json