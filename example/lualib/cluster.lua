local moon = require("moon")
local seri = require("seri")
local seri_packstr = seri.packstring

local M = {}

local clusterd

local call = moon.co_call_with_header

function M.call(rnode, rservice, ...)
    if not clusterd then
        clusterd = moon.unique_service("clusterd")
        if 0 == clusterd then
            return false,"clusterd service not start"
        end
    end
    return call("lua", clusterd,seri_packstr("CALL",rnode,rservice),...)
end

return M
