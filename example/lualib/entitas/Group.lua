local Delegate      = require("entitas.Delegate")
local set           = require("unorderset")
local set_insert    = set.insert
local set_remove    = set.remove
local set_has       = set.has
local set_size      = set.size
local Group = {}

--[[
Use context.get_group(matcher) to get a group of entities which
match the specified matcher. Calling context.get_group(matcher) with
the same matcher will always return the same instance of the group.

The created group is managed by the context and will always be up to
date. It will automatically add entities that match the matcher or
remove entities as soon as they don't match the matcher anymore.
]]

Group.__index = Group

Group.__tostring = function(t)
        return string.format("<Group [{%s}]>", tostring(t._matcher))
    end

function Group.new(matcher)

    local tb = {}
    -- Occurs when an entity gets added.
    tb.on_entity_added = Delegate.new()
    -- Occurs when an entity gets removed.
    tb.on_entity_removed = Delegate.new()
    -- Occurs when a component of an entity in the group gets
    -- replaced.
    tb.on_entity_updated = Delegate.new()
    tb._matcher = matcher
    tb.entities = set.new()
    return setmetatable(tb, Group)

end

--[[
Returns the only entity in this group.
It will return None if the group is empty.
It will throw a :class:`MissingComponent` if the group has more
than one entity.
]]
function Group:single_entity()
    local count = set.size(self.entities)

    if count == 1 then
        return self.entities:at(1)
    end

    if count == 0 then
        return nil
    end

    error(string.format("Cannot get a single entity from a group containing %d entities", count))
end

function Group:entity_size()
    return set_size(self.entities)
end

--[[
This is used by the context to manage the group.
:param matcher: Entity
]]
function Group:handle_entity_silently(entity)
    assert(entity)
    if self._matcher:matches(entity) then
        self:_add_entity_silently(entity)
    else
        self:_remove_entity_silently(entity)
    end
end

--[[
This is used by the context to manage the group.
:param matcher: Entity
]]
function Group:handle_entity(entity, component)
    if self._matcher:matches(entity) then
        --print("handle_entity match")
        self:_add_entity(entity, component)
    else
        --print("handle_entity not match",self._matcher)
        --print("entity",entity)
        self:_remove_entity(entity, component)
    end
end

--[[
This is used by the context to manage the group.
:param matcher: Entity
]]
function Group:update_entity(entity, previous_comp, new_comp)
    if set.has(self.entities, entity) then
        self.on_entity_removed(entity, previous_comp)
        self.on_entity_added(entity, new_comp)
        self.on_entity_updated(entity, previous_comp, new_comp)
    end
end

function Group:_add_entity_silently(entity)
    if not set.has(self.entities, entity) then
        set_insert(self.entities, entity)
        return true
    end
    return false
end

function Group:_add_entity(entity, component)
    local entity_added = self:_add_entity_silently(entity)
    if entity_added then
        --print("size", #self.on_entity_added._listeners)
        self.on_entity_added(entity, component)
    end
end

function Group:_remove_entity_silently(entity)
    if set_has(self.entities, entity) then
        set_remove(self.entities, entity)
        return true
    end
    --print("Group:_remove_entity_silently failed")
    return false
end

function Group:_remove_entity(entity, component)
    local entity_removed = self:_remove_entity_silently(entity)
    if entity_removed then
        self.on_entity_removed(entity, component)
    end
end

return Group
