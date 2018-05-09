--[[
entitas.entity
~~~~~~~~~~~~~~
An entity is a container holding data to represent certain
objects in your application. You can add, replace or remove data
from entities.

Those containers are called 'components'. They are represented by
namedtuples for readability.
]]
local table_unpack = table.unpack
local Delegate = require("entitas.Delegate")
local M = {}

M.__index = M

-- print string
M.__tostring = function(t)
    local str = ""
    for _, v in pairs(t._components) do
        if #str > 0 then
            str = str .. ",\n"
        end

        if v then
            str = str .. tostring(v)
        end
    end
    return string.format("\n<Entity_%d\n %s\n>", t._uid, str)
end

--[[
    Use context.create_entity() to create a new entity and
    context.destroy_entity() to destroy it.
    You can add, replace and remove components to an entity.
]]
function M.new()
    local tb = {}
    -- Occurs when a component gets added.
    tb.on_component_added = Delegate.new()
    -- Occurs when a component gets removed.
    tb.on_component_removed = Delegate.new()
    -- Occurs when a component gets replaced.
    tb.on_component_replaced = Delegate.new()
    -- Dictionary mapping component type and component instance.
    tb._components = {}
    -- Each entity has its own unique uid which will be
    -- set by the context when you create the entity.
    tb._uid = 0
    -- The context manages the state of an entity.
    -- Active entities are enabled, destroyed entities are not.
    tb._is_enabled = false
    return setmetatable(tb, M)
end

function M:activate(uid)
    self._uid = uid
    self._is_enabled = true
end

--[[
 Adds a component.
param comp_type: table type
param ...: (optional) data values
]]
function M:add(comp_type, ...)
    if not self._is_enabled then
        error("Cannot add component entity is not enabled.")
    end

    if self:has(comp_type) then
        error("Cannot add another component")
    end

    local new_comp = comp_type.new(...)
    self._components[comp_type] = new_comp
    self.on_component_added(self, new_comp)
end

function M:remove(comp_type)
    if not self._is_enabled then
        error("Cannot add component entity is not enabled.")
    end

    if not self:has(comp_type) then
        error(string.format("Cannot remove unexisting component %s", tostring(comp_type.__comp_name)))
    end

    --print("Entity:remove")
    self:_replace(comp_type, nil)
end

function M:replace(comp_type, ...)
    if not self._is_enabled then
        error("Cannot add component entity is not enabled.")
    end

    local args = {...} or {}

    if self:has(comp_type) then
        self:_replace(comp_type, args)
    else
        self:add(comp_type, ...)
    end
end

function M:_replace(comp_type, args)
    local previous_comp = self._components[comp_type]
    if not args then
        --print("_replace 0")
        self._components[comp_type] = nil
        self.on_component_removed(self, previous_comp)
    else
        local new_comp = comp_type.new(table_unpack(args))
        self._components[comp_type] = new_comp
        self.on_component_replaced(self, previous_comp, new_comp)
    end
end

function M:get(comp_type)
    if not self:has(comp_type) then
        error(string.format("Cannot remove unexisting component"))
    end
    return self._components[comp_type]
end

function M:has(...)
    local args = {...}
    if #args == 1 then
        return (self._components[args[1]] ~= nil)
    end

    for _, v in pairs(args) do
        if not self._components[v] then
            return false
        end
    end
    return true
end

function M:has_any(...)
    local args = {...}
    for _, v in pairs(args) do
        if self._components[v] then
            return true
        end
    end
    return false
end

function M:remove_all()
    for k, v in pairs(self._components) do
        if v then
            self:_replace(k, nil)
        end
    end
end

-- use context destroy entity
function M:destroy()
    self._is_enabled = false
    self:remove_all()
end


return M
