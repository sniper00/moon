local fs = require("fs")

local loaded = package.loaded
local searchpath = package.searchpath

local function import( name )
    local info = debug.getinfo(2, "S")
    local prefix = string.sub(info.source, 2, -1)
    prefix = fs.parent_path(prefix)
    prefix = fs.relative_work_path(prefix)
    local fullname = string.gsub( prefix.."/"..name,"[/\\]","." )
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