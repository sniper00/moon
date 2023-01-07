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

---Set option empty as array. default true.
---@param opt boolean 
function json.encode_empty_as_array(opt) end

---Set option encode number key. default true.
---@param opt boolean 
function json.encode_number_key(opt) end

---Set option concat buffer size.
---@param buffer_size integer @default 512
---@param buffer_head_size integer @default 16
function json.concat_buffer_size(buffer_size, buffer_head_size) end

return json
