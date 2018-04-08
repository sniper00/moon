local set           = require("unorderset")
local util           = require("util")
local set_insert    = set.insert
local set_remove    = set.remove
local class         = util.class

local AbstractEntityIndex = require("entitas.AbstractEntityIndex")

local EntityIndex = class("EntityIndex", AbstractEntityIndex)

function EntityIndex:ctor(comp_type, group, ...)
    EntityIndex.super.ctor(self, comp_type, group, ...)
end

function EntityIndex:get_entities(key)
    --print("key", key)
    if not self._indexes[key] then
        self._indexes[key] = set.new()
    end
    return self._indexes[key]
end

function EntityIndex:_add_entity(key, entity)
    --print("key", key, "entity", entity)
    local t = self:get_entities(key)
    set_insert(t, entity)
end

function EntityIndex:_remove_entity(key, entity)
    local t = self:get_entities(key)
    set_remove(t, entity)
end

return EntityIndex
