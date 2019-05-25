local strsub        = string.sub
local strgsub       = string.gsub
local strupper      = string.upper
local tbinsert      = table.insert

function string.split(input, delimiter)
    local resultStrList = {}
    strgsub(input,'[^'..delimiter..']+',function ( w )
        tbinsert(resultStrList,w)
    end)
    return resultStrList
end

local _TRIM_CHARS = " \t\n\r"

function string.ltrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    return strgsub(input, pattern, "")
end

function string.rtrim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "[" .. chars .. "]+$"
    return strgsub(input, pattern, "")
end

function string.trim(input, chars)
    chars = chars or _TRIM_CHARS
    local pattern = "^[" .. chars .. "]+"
    input = strgsub(input, pattern, "")
    pattern = "[" .. chars .. "]+$"
    return strgsub(input, pattern, "")
end

function string.ucfirst(input)
    return strupper(strsub(input, 1, 1)) .. strsub(input, 2)
end