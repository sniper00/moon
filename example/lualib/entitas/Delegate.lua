local set = require("unorderset")
local set_insert = set.insert
local set_remove = set.remove
local set_has    = set.has
local M = {}

M.__index = M

M.__call = function(t, ...)
    for k,_ in pairs(t._listeners) do
        k(...)
    end
end

function M.new()
    local tb = {}
    tb._listeners = set.new()
    return setmetatable(tb, M)
end

function M.add(self, f)
    assert(not set_has(self._listeners,f))
    set_insert(self._listeners, f)
end

function M.remove(self, f)
    return set_remove(self._listeners, f)
end

return M
