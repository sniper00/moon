
local M = {}

M.__index = M

function M.new()
    local t = {}
    t.cur = 0
    t.buffer = {}
    setmetatable(t,M)
    return t
end

function M.push(self, value)
    self.cur = self.cur + 1
    self.buffer[self.cur] = value
end

function M.pop(self)
    assert(self.cur>0,"array:attemp pop empty table")
    self.cur = self.cur - 1
    return self.buffer[self.cur+1]
end

function M.clear(self)
    self.cur = 0
end

function M.size(self)
    return self.cur
end

function M.foreach(self,f)
    for i=1,self.cur do
        f(self.buffer[i])
    end
end

return M
