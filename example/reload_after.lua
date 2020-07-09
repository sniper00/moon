local moon = require("moon")
local M = {}

M.__index = M

function M.new()
    local obj = {
        n = 0
    }
    return setmetatable(obj,M)
end

function M:func()
    print("after", self.n)
    self.n=self.n-1
    moon.sleep(1000)
end

return M