local string_sub        = string.sub
local string_gsub       = string.gsub
local string_upper      = string.upper
local table_insert      = table.insert

function string.split(input, delimiter)
    local resultStrList = {}
    string_gsub(input,'[^'..delimiter..']+',function ( w )
        table_insert(resultStrList,w)
    end)
    return resultStrList
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