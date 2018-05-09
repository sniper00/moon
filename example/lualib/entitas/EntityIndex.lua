local set           = require("unorderset")
local util           = require("util")
local set_insert    = set.insert
local set_remove    = set.remove
local class         = util.class

local AbstractEntityIndex = require("entitas.AbstractEntityIndex")

local M = class("EntityIndex", AbstractEntityIndex)

function M:ctor(comp_type, group, ...)
    M.super.ctor(self, comp_type, group, ...)
end

function M:get_entities(key)
    --print("key", key)
    if not self._indexes[key] then
        self._indexes[key] = set.new()
    end
    return self._indexes[key]
end

function M:_add_entity(key, entity)
    --print("key", key, "entity", entity)
    local t = self:get_entities(key)
    set_insert(t, entity)
end

function M:_remove_entity(key, entity)
    local t = self:get_entities(key)
    set_remove(t, entity)
end

return M
