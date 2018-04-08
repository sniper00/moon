local set           = require("unorderset")
local set_insert    = set.insert
local set_remove    = set.remove
local set_size      = set.size
local set_has       = set.has
local table_insert  = table.insert
local table_remove  = table.remove

local Entity        = require("entitas.Entity")
local Group         = require("entitas.Group")
local Matcher       = require("entitas.Matcher")

--[[
    The Context is the factory where you create and destroy entities.
    Use it to filter entities of interest.
]]
local Context = {}

Context.__index = Context

function Context.new()

    local tb = {}
    -- Entities retained by this context.
    tb.entities = set.new()
    -- An object pool to recycle entities.
    tb._entities_pool = {}
    -- Entities counter
    tb._uuid = 1
    -- Dictionary of matchers mapping groups.
    tb._groups = {}
    tb._entity_indices = {}
    tb.comp_added_or_removed = function(...) return tb._comp_added_or_removed(tb, ...) end
    tb.comp_replaced = function(...) return tb._comp_replaced(tb, ...) end
    return setmetatable(tb, Context)
end

-- Checks if the context contains this entity.
function Context:has_entity(entity)
    return set_has(self.entities,entity)
end

--[[
Creates an entity. Pop one entity from the pool if it is not
empty, otherwise creates a new one. Increments the entity index.
Then adds the entity to the list.
:rtype: Entity
]]
function Context:create_entity()
    local entity = table_remove(self._entities_pool)
    if not entity then
        entity = Entity.new(self.componentPools)
        entity.on_component_added:add(self.comp_added_or_removed)
        entity.on_component_removed:add(self.comp_added_or_removed)
        entity.on_component_replaced:add(self.comp_replaced)
    end

    entity:activate(self._uuid)
    self._uuid = self._uuid + 1
    set_insert(self.entities, entity)
    return entity
end

--[[
Removes an entity from the list and add it to the pool. If
the context does not contain this entity, a
:class:`MissingEntity` exception is raised.
:param entity: Entity
]]
function Context:destroy_entity(entity)
    if not self:has_entity(entity) then
        error("The context does not contain this entity.")
    end

    entity:destory()

    set_remove(self.entities, entity)
    table_insert(self._entities_pool, entity)
end

function Context:entity_size()
    return set_size(self.entities)
end

--[[
User can ask for a group of entities from the context. The
group is identified through a :class:`Matcher`.
:param entity: Matcher
]]
function Context:get_group(matcher)
    local group = self._groups[matcher]
    if group then
        return group
    end

    group = Group.new(matcher)

    self.entities:foreach(function(v)
        assert(v)
        group:handle_entity_silently(v)
    end)

    self._groups[matcher] = group

    return group
end

function Context:set_unique_component(comp_type, ...)
    local entity = self:create_entity()
    entity:add(comp_type, ...)
end

function Context:get_unique_component(comp_type)
    local group = self:get_group(Matcher({comp_type}))
    local entity = group:single_entity()
    return entity:get(comp_type)
end

function Context:add_entity_index(entity_index)
    self._entity_indices[entity_index.comp_type] = entity_index
end

function Context:get_entity_index(comp_type)
    return self._entity_indices[comp_type]
end

function Context:_comp_added_or_removed(entity, comp)
    for _, group in pairs(self._groups) do
        --print("comp_added_or_removed",entity,comp)
        group:handle_entity(entity, comp)
    end
end

function Context:_comp_replaced(entity, previous_comp, new_comp)
    for _, group in pairs(self._groups) do
        --print("update",entity,previous_comp,new_comp)
        group:update_entity(entity, previous_comp, new_comp)
    end
end

return Context
