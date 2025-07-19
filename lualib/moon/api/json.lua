---@meta

error("DO NOT REQUIRE THIS FILE")

---@class json
---@field null lightuserdata @ JSON null value (lightuserdata pointing to nullptr)
local json = {}

--- Encode Lua value to JSON string
---@param t table|number|string|boolean|nil @ Value to encode
---@return string @ JSON formatted string
---@nodiscard
function json.encode(t) end

--- Encode Lua value to pretty-formatted JSON string
---@param t table|number|string|boolean|nil @ Value to encode
---@return string @ Pretty-formatted JSON string
---@nodiscard
function json.pretty_encode(t) end

--- Decode JSON string to Lua value
---@param str string|cstring_ptr @ JSON string or C string pointer
---@param n? integer @ Length in bytes (required for cstring_ptr)
---@return table|number|string|boolean|lightuserdata @ Decoded Lua value
---@nodiscard
function json.decode(str, n) end

--- Concatenate array elements or string into buffer
---@param array any[] @ Array of values to concatenate
---@return buffer_ptr @ Buffer object
---@overload fun(str:string):buffer_ptr
---@nodiscard
function json.concat(array) end

--- Concatenate parameters as Redis RESP protocol format
---@param ... any @ Arguments to encode as Redis protocol
---@return buffer_ptr @ Buffer containing RESP data
---@return integer @ Hash value for cluster routing
---@nodiscard
function json.concat_resp(...) end

---@alias json_options
---| 'encode_empty_as_array' # Empty tables as arrays `[]` instead of objects `{}`. Default: true
---| 'enable_number_key' # Numeric string keys become numeric keys on decode. Default: true  
---| 'enable_sparse_array' # Sparse arrays encode as arrays with null values. Default: false
---| 'has_metatfield' # Check `__object`/`__array` metafields for type hints. Default: true
---| 'concat_buffer_size' # Initial buffer size for concat operations. Must be >= 64. Default: 512

--- Configure JSON encoding/decoding options
---@param option_name json_options @ Option name
---@param new_value? any @ New value (omit to get current)
---@return any @ Previous value
function json.options(option_name, new_value) end

--- Create or mark table as JSON object
---@param v? integer|table @ Capacity hint or table to mark
---@return table @ Table marked as JSON object
function json.object(v) end

--- Create or mark table as JSON array
---@param v? integer|table @ Capacity hint or table to mark
---@return table @ Table marked as JSON array
function json.array(v) end

return json
