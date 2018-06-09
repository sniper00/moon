local string_byte       = string.byte
local string_find       = string.find
local string_format     = string.format
local string_sub        = string.sub
local string_gsub       = string.gsub
local string_len        = string.len
local string_upper      = string.upper
local table_insert      = table.insert

local M = {}

-------------------------------TABLE-----------------------------------------

local _copy
_copy = function(t, lookup)
    if type(t) ~= "table" then
        return t
    elseif lookup[t] then
        return lookup[t]
    end
    local n = {}
    lookup[t] = n
    for key, value in pairs(t) do
        n[_copy(key, lookup)] = _copy(value, lookup)
    end
    return n
end

function M.table_copy(t)
    local lookup = {}
    return _copy(t, lookup)
end

function M.table_keys(hashtable)
    local keys = {}
    for k, _ in pairs(hashtable) do
        keys[#keys + 1] = k
    end
    return keys
end

function M.table_values(hashtable)
    local values = {}
    for _, v in pairs(hashtable) do
        values[#values + 1] = v
    end
    return values
end

function M.table_merge(dest, src)
    for k, v in pairs(src) do
        dest[k] = v
    end
end

function M.table_map(t, fn)
    local n = {}
    for k, v in pairs(t) do
        n[k] = fn(v, k)
    end
    return n
end

function M.table_walk(t, fn)
    for k,v in pairs(t) do
        fn(v, k)
    end
end

function M.table_filter(t, fn)
    local n = {}
    for k, v in pairs(t) do
        if fn(v, k) then
            n[k] = v
        end
    end
    return n
end

function M.table_length(t)
    local count = 0
    for _, _ in pairs(t) do
        count = count + 1
    end
    return count
end

function M.table_readonly(t, name)
    name = name or "table"
    setmetatable(t, {
        __newindex = function()
            error(string_format("<%s:%s> is readonly table", name, tostring(t)))
        end,
        __index = function(_, key)
            error(string_format("<%s:%s> not found key: %s", name, tostring(t), key))
        end
    })
    return t
end

function M.table_equal(t1,t2)
    for k,v in pairs(t1) do
        if t2[k] ~= v then
            return false
        end
    end
    return true
end

function M.table_contains( arraytable, value )
    for k,v in pairs(arraytable) do
        if v == value then
            return true
        end
    end
    return false
end

function M.table_intersect( t1, t2 )
    for k,v in pairs(t1) do
        if t2[k] == v then
            return true
        end
    end
    return false
end


----------------------------------STRING---------------------------------------

-- string ext

function M.string_split(input, delimiter)
    input = tostring(input)
    delimiter = tostring(delimiter)
    if (delimiter == "") then return false end
    local pos,arr = 1, {}
    for st, sp in function() return string_find(input, delimiter, pos, true) end do
        local str = string_sub(input, pos, st - 1)
        if str ~= "" then
            table_insert(arr, str)
        end
        pos = sp + 1
    end
    if pos <= string_len(input) then
        table_insert(arr, string_sub(input, pos))
    end
    return arr
end

local _TRIM_CHARS = " \t\n\r"

function M.string_ltrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    return string_gsub(input, pattern, "")
end

function M.string_rtrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "[" .. chars .. "]+$"
    return string_gsub(input, pattern, "")
end

function M.string_trim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    input = string_gsub(input, pattern, "")
    pattern = "[" .. chars .. "]+$"
    return string_gsub(input, pattern, "")
end

function M.string_ucfirst(input)
    return string_upper(string_sub(input, 1, 1)) .. string_sub(input, 2)
end

---------------------------------------------------------------------------

local math_ceil  = math.ceil
local math_floor = math.floor
function M.math_round(value)
    value = tonumber(value) or 0
    return math_floor(value + 0.5)
end

function M.math_trunc(x)
    if x <= 0 then
        return math_ceil(x)
    end
    if math_ceil(x) == x then
        x = math_ceil(x)
    else
        x = math_ceil(x) - 1
    end
    return x
end

function M.math_newrandomseed()
    math.randomseed(os.clock())
    math.random()
    math.random()
    math.random()
    math.random()
end

--------------------------------------------------------------------------
function M.gettimezone()
    local now = os.time()
    return os.difftime(now, os.time(os.date("!*t", now))) / 3600
end

function M.gettime(date, utc)
    local t = os.time({
        year  = date[1],
        month = date[2],
        day   = date[3],
        hour  = date[4],
        min   = date[5],
        sec   = date[6],
    })
    if utc ~= false then
        local now = os.time()
        local offset = os.difftime(now, os.time(os.date("!*t", now)))
        t = t + offset
    end
    return t
end

function M.exists(path)
    local file = M.open(path, "rb")
    if file then
        M.close(file)
        return true
    end
    return false
end

function M.readfile(path)
    local file = io.open(path, "rb")
    if file then
        local content = file:read("*a")
        io.close(file)
        return content
    end
    return nil
end

function M.writefile(path, content, mode)
    mode = mode or "w+b"
    local file = io.open(path, mode)
    if file then
        if file:write(content) == nil then
            return false
        end
        io.close(file)
        return true
    else
        return false
    end
end

function M.pathinfo(path)
    local pos = string_len(path)
    local extpos = pos + 1
    while pos > 0 do
        local b = string_byte(path, pos)
        if b == 46 then -- 46 = char "."
            extpos = pos
        elseif b == 47 then -- 47 = char "/"
            break
        end
        pos = pos - 1
    end

    local dirname  = string_sub(path, 1, pos)
    local filename = string_sub(path, pos + 1)

    extpos = extpos - pos
    local basename = string_sub(filename, 1, extpos - 1)
    local extname  = string_sub(filename, extpos)

    return {
        dirname  = dirname,
        filename = filename,
        basename = basename,
        extname  = extname
    }
end

function M.filesize(path)
    local size = false
    local file = M.open(path, "r")
    if file then
        local current = file:seek()
        size = file:seek("end")
        file:seek("set", current)
        M.close(file)
    end
    return size
end


--------------------------------CLASS-------------------------------------------

local _iskindofinternal
_iskindofinternal = function(mt, classname)
    if not mt then return false end

    local index = rawget(mt, "__index")
    if not index then return false end

    local cname = rawget(index, "__cname")
    if cname == classname then return true end

    return _iskindofinternal(getmetatable(index), classname)
end

function M.iskindof(target, classname)
    local targetType = type(target)
    if targetType ~= "table" then
        return false
    end
    return _iskindofinternal(getmetatable(target), classname)
end

function M.class(classname, super)
    assert(type(classname) == "string", string_format("class() - invalid class name \"%s\"", tostring(classname)))
    local superType = type(super)
    local cls
    if not super or superType == "table" then
        -- inherited from Lua Object
        if super then
            cls = {}
            setmetatable(cls, {__index = super})
            cls.super = super
        else
            cls = {ctor = function() end,__gc = true}
        end

        cls.__cname = classname
        cls.__index = cls

        function cls.new(...)
            local instance = setmetatable({}, cls)
            instance.class = cls
            instance:ctor(...)
            return instance
        end
    else
        error(string.format("class() - create class \"%s\" with invalid super type",classname), 0)
    end
    return cls
end

function M.handler(target, method)
    return function(...)
        return method(target, ...)
    end
end

function M.checknumber(value, base)
    return tonumber(value, base) or 0
end

function M.checkint(value)
    value = tonumber(value) or 0
    return math_floor(value + 0.5)
end

function M.checkbool(value)
    return (value ~= nil and value ~= false)
end

function M.checktable(value)
    if type(value) ~= "table" then value = {} end
    return value
end

return M