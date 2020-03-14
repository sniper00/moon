local moon = require("moon")

local status = coroutine.status
local running = coroutine.running
local tbinsert = table.insert
local tbremove = table.remove

local M = {}

M.__index = M

function M.new()
    local obj = {}
    obj.q = {}
    return setmetatable(obj,M)
end

function M:run(fn, err_fn)
    tbinsert(self.q, fn)
    if not self.thread or status(self.thread)== "dead" then
        -- if self.thread and status(self.thread)== "dead" then
        --     print("..............................", self.thread )
        -- end
        moon.async(function()
            self.thread = running()
            while #self.q >0 do
                local fn_ = tbremove(self.q,1)
                local ok, errmsg = xpcall(fn_, debug.traceback)
                if ok == false  then
                    if err_fn then
                     err_fn(errmsg)
                    else
                        moon.error(errmsg)
                    end
                end
            end
            self.thread = nil
        end)
    end
end

function M:size()
    return #self.q
end

return M