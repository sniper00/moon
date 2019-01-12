local string_find       = string.find
local string_sub        = string.sub
local string_gsub       = string.gsub
local string_len        = string.len
local string_upper      = string.upper
local table_insert      = table.insert

function string.split(input, delimiter)
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

function string.ltrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    return string_gsub(input, pattern, "")
end

function string.rtrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "[" .. chars .. "]+$"
    return string_gsub(input, pattern, "")
end

function string.trim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    input = string_gsub(input, pattern, "")
    pattern = "[" .. chars .. "]+$"
    return string_gsub(input, pattern, "")
end

function string.ucfirst(input)
    return string_upper(string_sub(input, 1, 1)) .. string_sub(input, 2)
end