local util          = require("util")
local class         = util.class

local AbstractEntityIndex = require("entitas.AbstractEntityIndex")

local M = class("PrimaryEntityIndex", AbstractEntityIndex)

function M:ctor(comp_type, group, ...)
    M.super.ctor(self, comp_type, group, ...)
end

function M:get_entity(key)
    return self._indexes[key]
end

function M:_add_entity(key, entity)
    if self._indexes[key] then
        error(string.format("Entity for key '%s' already exists ! Only one entity for a primary key is allowed.", key))      
    end
    self._indexes[key] = entity
end

function M:_remove_entity(key, _)
    self._indexes[key] = nil
end

return M
