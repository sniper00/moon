local fs = require("fs")

local loaded = package.loaded
local searchpath = package.searchpath

local function import( name )
    local info = debug.getinfo(2, "S")
    local prefix = string.sub(info.source, 2, -1)
    prefix = fs.parent_path(prefix)
    prefix = fs.relative_work_path(prefix)
    local fullname = string.gsub( prefix.."/"..name,"[/\\]","." )
    local pos = 1
    while true do
        if fullname:byte(pos) ~= string.byte(".") then
            if pos > 1 then
                fullname = fullname:sub(pos)
            end
            break
        end
        pos = pos + 1
    end
    local m = loaded[fullname] or loaded[name]
    if m then
        return m
    end
    if searchpath(fullname, package.path) then
        return require(fullname)
    else
        return require(name)
    end
end

return import