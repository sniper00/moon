local moon = require("moon")
local seri = require("seri")
local packs = seri.packs
local pack = seri.pack
local co_yield = coroutine.yield

local M = {}

local clusterd

local send = moon.raw_send


function M.call(rnode, rservice, ...)
    if not clusterd then
        clusterd = moon.unique_service("clusterd")
        if 0 == clusterd then
            return false, "clusterd service not start"
        end
    end

    local responseid,err = moon.make_response(clusterd)
    if not responseid then
        return false, err
    end

    send('lua',clusterd,packs(rnode, rservice,"CALL"),pack(...),responseid)
    return co_yield()
end

return M
