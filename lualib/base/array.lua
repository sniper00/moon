
local M = {}

M.__index = M

function M.new(isweak)
    local t = {}
    t._cur = 0
    t._data = {}
    if isweak then
        setmetatable(t._data,{__mode ="v"})
    end
    setmetatable(t,M)
    return t
end

function M.push(self, value)
    self._cur = self._cur + 1
    self._data[self._cur] = value
end

function M.pop(self)
    assert(self._cur>0,"array:attemp pop empty table")
    self._cur = self._cur - 1
    return self._data[self._cur+1]
end

function M.clear(self)
    self._cur = 0
end

function M.size(self)
    return self._cur
end

function M.data(self)
    return self._data
end

function M.foreach(self,f)
    for i=1,self._cur do
        f(self._data[i])
    end
end

return M
