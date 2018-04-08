local set = require("unorderset")
local set_insert = set.insert
local set_remove = set.remove
local set_has    = set.has
local Delegate = {}

Delegate.__index = Delegate

Delegate.__call = function(t, ...)
    for k,_ in pairs(t._listeners) do
        k(...)
    end
end

function Delegate.new()
    local tb = {}
    tb._listeners = set.new()
    return setmetatable(tb, Delegate)
end

function Delegate.add(self, f)
    assert(not set_has(self._listeners,f))
    set_insert(self._listeners, f)
end

function Delegate.remove(self, f)
    return set_remove(self._listeners, f)
end

return Delegate
