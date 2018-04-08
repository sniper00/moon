local util          = require("util")
local class         = util.class

local AbstractEntityIndex   = class("AbstractEntityIndex")

function AbstractEntityIndex:ctor(comp_type, group, ...)
    self.comp_type = comp_type
    self._group = group
    self._fields = {...}
    self._indexes = {}
    self.on_entity_added = function(...) return self._on_entity_added(self, ...) end
    self.on_entity_removed = function(...) return self._on_entity_removed(self, ...) end
    self:_activate()

    local mt = getmetatable(self)
    mt.__gc = function(t) t:_deactivate() end
end

function AbstractEntityIndex:_activate()
    self._group.on_entity_added:add(self.on_entity_added)
    self._group.on_entity_removed:add(self.on_entity_removed)
    self:_index_entities()
end

function AbstractEntityIndex:_deactivate()
    self._group.on_entity_added:remove(self.on_entity_added)
    self._group.on_entity_removed:remove(self.on_entity_removed)
    self._indexes = {}
end

function AbstractEntityIndex:_index_entities()
    self._group.entities:foreach(function(entity)
        local comp_type = entity:get(self.comp_type)
        for _, field in pairs(self._fields) do
            self:_add_entity(comp_type[field], entity)
        end
    end)
end

function AbstractEntityIndex:_on_entity_added(entity, component)
    --print("entity",entity, "component", component)
    for _, field in pairs(self._fields) do
        self:_add_entity(component[field], entity)
    end
end

function AbstractEntityIndex:_on_entity_removed(entity, component)
    for _, field in pairs(self._fields) do
        self:_remove_entity(component[field], entity)
    end
end

function AbstractEntityIndex:_add_entity(key, entity)
    error("not imp")
end

function AbstractEntityIndex:_remove_entity(key, entity)
    error("not imp")
end

return AbstractEntityIndex
