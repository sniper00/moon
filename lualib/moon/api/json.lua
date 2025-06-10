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
---| 'encode_empty_as_array' @ Controls whether empty tables are encoded as JSON arrays (`[]`) instead of objects (`{}`). Default: true
---| 'enable_number_key' @ Controls whether tables with numeric keys are encoded as JSON objects with string keys. Default: true
---| 'enable_sparse_array' @ Controls whether sparse arrays (with gaps in indices) are encoded as arrays with null values. Default: false
---| 'concat_buffer_size' @ Sets the buffer size and buffer head size for json.concat operations. Requires two integer values. Default: 512, 16
---| 'has_metafield' @ Controls whether to check `__object` and `__array` metafields during encoding/decoding. Default: true

---Sets JSON encoding/decoding options and returns their previous values
---@param option_name json_options @ The name of the option to set
---@param ... any @ The new value(s) for the specified option
---@return any ... @ The previous value(s) of the specified option
function json.options(option_name, ...) end


---Creates or marks a table as a JSON object
---@param v? integer|table @ Either initial capacity (integer) or table to be marked as object
---@return table @ Table marked as JSON object with metatable containing __object=true
function json.object(v) end

---Creates or marks a table as a JSON array
---@param v? integer|table @ Either initial capacity (integer) or table to be marked as array
---@return table @ Table marked as JSON array with metatable containing __array=true
function json.array(v) end
    


return json
