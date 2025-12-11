local strlen   = string.len
local dbgtrace = debug.traceback
local strrep   = string.rep
local strsplit = string.split
local strtrim  = string.trim
local tsort    = table.sort
local tbconcat = table.concat
local tbinsert = table.insert

local pairs = pairs
local ipairs = ipairs
local tostring = tostring
local type = type

local function _dump_value(v)
    if type(v) == "string" then
        v = "\"" .. v .. "\""
    end
    return tostring(v)
end

local function _dump_key(v)
    if type(v) == "number" then
        v = "[" .. v .. "]"
    end
    return tostring(v)
end

local MAX_NESTING <const> = 32

---@param any any
---@param isreturn? boolean
_G["print_r"] = function(any, isreturn)
    local lookup = {}
    local result = {}

    if not isreturn then
        local traceback = strsplit(dbgtrace("", 2), "\n")
        tbinsert(result, "dump from: " .. strtrim(traceback[2]) .. "\n")
    end

    local function _dump(key, value, indent, nest, keylen)
        local space = ""
        if type(keylen) == "number" then
            space = strrep(" ", keylen - strlen(_dump_key(key)))
        end

        if type(value) ~= "table" then
            if key then
                result[#result + 1] = indent
                result[#result + 1] = _dump_key(key)
                result[#result + 1] = space
                result[#result + 1] = " = "
            end
            result[#result + 1] = _dump_value(value)
            result[#result + 1] = ",\n"
        elseif lookup[tostring(value)] then
            if key then
                result[#result + 1] = indent
                result[#result + 1] = _dump_key(key)
                result[#result + 1] = space
                result[#result + 1] = " = "
            end
            result[#result + 1] = "'*REF*'"
            result[#result + 1] = ",\n"
        else
            lookup[tostring(value)] = true
            if nest > MAX_NESTING then
                result[#result + 1] = indent
                result[#result + 1] = _dump_key(key)
                result[#result + 1] = " = "
                result[#result + 1] = "'*MAX NESTING*'"
                result[#result + 1] = ",\n"
            else
                if key then
                    result[#result + 1] = indent
                    result[#result + 1] = _dump_key(key)
                    result[#result + 1] = " = {\n"
                else
                    result[#result + 1] = "{\n"
                end

                local indent2 = indent .. "    "
                local keys = {}
                keylen = 0
                local values = {}
                for k, v in pairs(value) do
                    keys[#keys + 1] = k
                    local vk = _dump_value(k)
                    local vkl = strlen(vk)
                    if vkl > keylen then keylen = vkl end
                    values[k] = v
                end
                tsort(keys, function(a, b)
                    if type(a) == "number" and type(b) == "number" then
                        return a < b
                    else
                        return tostring(a) < tostring(b)
                    end
                end)
                for _, k in ipairs(keys) do
                    _dump(k, values[k], indent2, nest + 1, keylen)
                end
                result[#result + 1] = indent .. "},\n"
            end
        end
    end

    _dump(nil, any, "", 1)

    local last = result[#result]
    result[#result] = string.sub(last, 1, #last - 2) .. '\n'

    if isreturn then
        return tbconcat(result)
    else
        print(tbconcat(result))
    end
end

_G["cast_int"] = function(x)
    x = math.tointeger(x)
    if not x then
        error("cast_int failed:" .. tostring(x))
    end
    return x
end