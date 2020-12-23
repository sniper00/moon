local moon = require("moon")

local status = coroutine.status
local running = coroutine.running

local task_queue = { h = 1, t = 0 }

local traceback = debug.traceback

local M = {}

M.__index = M

function M.new()
    local obj = {}
    obj.q = {}
    return setmetatable(obj,M)
end

function M:run(fn, err_fn)
    local t = task_queue.t + 1
	task_queue.t = t
	task_queue[t] = fn
    if not self.thread or status(self.thread)== "dead" then
        -- if self.thread and status(self.thread)== "dead" then
        --     print("..............................", self.thread )
        -- end
        moon.async(function()
            self.thread = running()
            while true do
                if task_queue.h > task_queue.t then
                    -- queue is empty
                    task_queue.h = 1
                    task_queue.t = 0
                    break
                end

                -- pop queue
                local h = task_queue.h
                local fn_ = task_queue[h]
                task_queue[h] = nil
                task_queue.h = h + 1

                local ok, errmsg = xpcall(fn_, traceback)
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