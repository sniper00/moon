-- 无序set
local unorderset = {}

unorderset.__index = unorderset

function unorderset.new()
    local t = {}
    setmetatable(t,unorderset)
    return t
end

function unorderset.insert(self, value)
    assert(not self[value],"set value already exist")
    self[value] = true
end

function unorderset.remove(self, value)
    self[value] = nil
    return value
end

function unorderset.size(self)
    local n = 0
    for _, _ in pairs(self) do
        n = n + 1
    end
    return n
end

function unorderset.has(self,value)
    return self[value]
end

function unorderset.foreach(self,f)
    for k, _ in pairs(self) do
        f(k)
    end
end

function unorderset.at(self,pos)
    local n = 0
    for k, _ in pairs(self) do
        n = n + 1
        if n == pos then
            return k
        end
    end
    return nil
end

return unorderset
