-- 无序set
local M = {}

M.__index = M

function M.new()
    local t = {}
    setmetatable(t,M)
    return t
end

function M.insert(self, value)
    assert(not self[value],string.format( "set value %s already exist",tostring(value)))
    self[value] = true
end

function M.remove(self, value)
    self[value] = nil
    return value
end

function M.size(self)
    local n = 0
    for _, _ in pairs(self) do
        n = n + 1
    end
    return n
end

function M.has(self,value)
    return self[value]
end

function M.foreach(self,f)
    for k, v in pairs(self) do
        if v then
            f(k)
        end
    end
end

function M.at(self,pos)
    local n = 0
    for k, _ in pairs(self) do
        n = n + 1
        if n == pos then
            return k
        end
    end
    return nil
end

function M.clear(self)
    for k, _ in pairs(self) do
        self[k] = nil
    end
end

return M
