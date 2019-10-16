local moon = require("moon")

local status = coroutine.status
local tbinsert = table.insert
local tbremove = table.remove
local running = coroutine.running

local M = {}

M.__index = M

function M.new()
    local obj = {}
    obj.q = {}
    return setmetatable(obj,M)
end

function M:run(f)
    tbinsert(self.q, f)
    if not self.thread or status(self.thread)== "dead" then
        -- if self.thread and status(self.thread)== "dead" then
        --     print("..............................", self.thread )
        -- end
        moon.async(function()
            self.thread = running()
            while #self.q >0 do
                local func = tbremove(self.q,1)
                func()
            end
            self.thread = nil
        end)
    end
end

return M