local json = require "json"
local buffer = require "buffer"

---@class KeyValueModel
---@field private _data table
---@field private _modified table
local M = {}

M.__index = function(t, k)
    local v = rawget(t, "_data")[k]
    t._modified[k] = v
    return v
end

M.__newindex = function(t, k, v)
    local data = rawget(t, "_data")
    if  data[k] == nil then
        error("key not found:" .. k)
    end
    rawset(data, k, v)
    t._modified[k] = v
end

local function make_one_kv(arr, key, value)
    if type(value) == "string" then
        value = json.encode(value)
    end

    local n = #arr
    arr[n + 1] = ",'"
    arr[n + 2] = key
    arr[n + 3] = "','"
    arr[n + 4] = value
    arr[n + 5] = "'"
end

function M.new(tbname, pk, data)
    local o = {}
    o._data = data
    o._modified = {}
    o._name = tbname
    o._pk = pk
    o._begin = string.format("SELECT update_%s(%s", tbname, pk)
    return setmetatable(o, M)
end

function M.modifyed(t)
    local modified = t._modified
    if next(modified) == nil then
        return ""
    end
    t._modified = {}

    local arr = {}
    arr[#arr + 1] = t._begin

    for key, value in pairs(modified) do
        make_one_kv(arr, key, value)
    end
    arr[#arr + 1] = ");"

    return buffer.unpack(json.concat(arr))
end

return M

