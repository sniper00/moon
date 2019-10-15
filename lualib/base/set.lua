-- 无序set
local M = {}

M.__index = M

function M.new(isweak)
    local t = {}
    t._data = {}
    t._size = 0
    if isweak then
        setmetatable(t._data, {__mode="k"})
    end
    setmetatable(t, M)
    return t
end

function M.insert(self, value)
    -- assert(not self._data[value], string.format("set: value %s already exist", tostring(value)))
    if not self._data[value] then
        self._data[value] = true
        self._size = self._size + 1
        return true
    end
    return false
end

function M.remove(self, value)
    --assert(self._data[value], "set:attemp remove unexist value")
    if self._data[value] then
        self._data[value] = nil
        self._size = self._size - 1
        return value
    end
    return false
end

function M.size(self)
    return self._size
end

function M.has(self, value)
    return self._data[value]
end

function M.foreach(self, f, ...)
    for k, v in pairs(self._data) do
        if v then
            f(k, ...)
        end
    end
end

function M.at(self, pos)
    local n = 0
    for k, _ in pairs(self._data) do
        n = n + 1
        if n == pos then
            return k
        end
    end
    return nil
end

function M.clear(self)
    for k, _ in pairs(self._data) do
        self._data[k] = nil
    end
    self._size = 0
end

return M
