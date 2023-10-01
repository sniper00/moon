---@meta

error("DO NOT REQUIRE THIS FILE")

---@class json
---@field null lightuserdata @ Represents json "null" values
local json = {}

---@param t table|number|string|boolean
---@return string
function json.encode(t) end

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

--- concat params as redis protocol, table type will encode to json string
---@return buffer_ptr
function json.concat_resp(...) end

---@alias json_options
---| 'encode_empty_as_array' @ Set option encode empty table as array. default true.
---| 'enable_number_key' @ Set option enable convert table's integer key as json object's key. default true.
---| 'enable_sparse_array' @ Set option enable sparse array. default false.
---| 'concat_buffer_size' @ Set option concat buffer_size(integer), buffer_head_size(integer). default 512 and 16.

---Set json options and return old values
---@param json_options json_options
---@return ...
function json.options(json_options, ...) end

return json
