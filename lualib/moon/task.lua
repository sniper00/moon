local moon = require("moon")
local corunning = coroutine.running
local costatus = coroutine.status
local coresume = coroutine.resume
local coyield = coroutine.yield

local traceback = debug.traceback

local ipairs = ipairs

local xpcall = xpcall

local _M = {}

---
--- 等待多个异步调用，并获取结果
function _M.wait_all(fnlist)
    local n = #fnlist
    local res = {}
    local co = corunning()
    for i,fn in ipairs(fnlist) do
        moon.async(function ()
            res[i] = {xpcall(fn, traceback)}
            n=n-1
            if costatus(co) == "suspended" then
                coresume(co)
            end
        end)
    end

    while true do
        if n==0 then
            break
        end
        coyield()
    end
    return res
end

return _M