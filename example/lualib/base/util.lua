
local math_floor = math.floor
local string_format     = string.format
local string_len        = string.len
local debug_traceback   = debug.traceback
local string_rep        = string.rep
local string_split      = string.split
local string_trim       = string.trim
local tsort             = table.sort

local tonumber = tonumber
local tostring = tostring
local type = type

function checknumber(value, base)
    return tonumber(value, base) or 0
end

function checkint(value)
    value = tonumber(value) or 0
    return math_floor(value + 0.5)
end

function checkbool(value)
    return (value ~= nil and value ~= false)
end

function checktable(value)
    if type(value) ~= "table" then value = {} end
    return value
end

function unused()
    -- body
end

function get_script_path()
    local info = debug.getinfo(2, "S")
    local path = info.source
    path = string.sub(path, 2, -1)
    return string.match(path, "^.*[/\\]")
end

local function _dump_value(v)
    if type(v) == "string" then
        v = "\"" .. v .. "\""
    end
    return tostring(v)
end

function print_r(value, desciption, nesting, _print)
    if type(nesting) ~= "number" then nesting = 10 end
    _print = _print or print

    local lookup = {}
    local result = {}
    local traceback = string_split(debug_traceback("", 2), "\n")
    table.insert(result,"\ndump from: " .. string_trim(traceback[2]).."\n")

    local function _dump(_value, _desciption, _indent, nest, keylen)
        _desciption = _desciption or "<var>"
        local spc = ""
        if type(keylen) == "number" then
            spc = string_rep(" ", keylen - string_len(_dump_value(_desciption)))
        end
        if type(_value) ~= "table" then
            result[#result + 1] = string_format("%s%s%s = %s", _indent, _dump_value(_desciption),
            spc, _dump_value(_value))
        elseif lookup[tostring(_value)] then
            result[#result + 1] = string_format("%s%s%s = *REF*", _indent, _dump_value(_desciption), spc)
        else
            lookup[tostring(_value)] = true
            if nest > nesting then
                result[#result + 1] = string_format("%s%s = *MAX NESTING*", _indent, _dump_value(_desciption))
            else
                result[#result + 1] = string_format("%s%s = {", _indent, _dump_value(_desciption))
                local indent2 = _indent .. "    "
                local keys = {}
                keylen = 0
                local values = {}
                for k, v in pairs(_value) do
                    keys[#keys + 1] = k
                    local vk = _dump_value(k)
                    local vkl = string_len(vk)
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
                    _dump(values[k], k, indent2, nest + 1, keylen)
                end
                result[#result + 1] = string_format("%s}", _indent)
            end
        end
    end
    _dump(value, desciption, "- ", 1)
    _print(table.concat(result,"\n"))
end

