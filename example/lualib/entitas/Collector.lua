local set           = require("unorderset")
local GroupEvent    = require("entitas.GroupEvent")
local set_insert    = set.insert
local set_remove    = set.remove

local M  = {}
M.__index = M

function M.new(groups)
    local tb = {}
    tb.entities = set.new()
    tb._groups = groups
    tb.add_entity = function(...) return tb._add_entity(tb, ...) end
    tb.remove_entity = function(...) return tb._remove_entity(tb, ...) end
    tb = setmetatable(tb, M)
    tb.activate(tb)
    return tb
end

--Activates the Collector and will start collecting
--changed entities. Collectors are activated by default.
function M:activate()
    for group, group_event in pairs(self._groups) do
        if group_event & GroupEvent.ADDED then
            group.on_entity_added:remove(self.add_entity)
            group.on_entity_added:add(self.add_entity)
        end

        if group_event & GroupEvent.REMOVED then
            group.on_entity_removed:remove(self.remove_entity)
            group.on_entity_removed:add(self.remove_entity)
        end

        if group_event & GroupEvent.UPDATE then
            group.on_entity_updated:remove(self.add_entity)
            group.on_entity_updated:add(self.add_entity)
        end
    end
end

function M:deactivate()
    for group, _ in pairs(self._groups) do
        group.on_entity_added:remove(self.add_entity)
        group.on_entity_removed:remove(self.add_entity)
    end

    self:clear_entities()
end

function M:clear_entities()
    self.entities:clear()
end

function M:_add_entity(entity)
    if self.entities:has(entity) then
        assert(false,string.format( "Collect already has Entity %s",tostring(entity)))
    end
    set_insert(self.entities, entity)
end

function M:_remove_entity(entity)
    -- if not self.entities:has(entity) then
    --     assert(false,string.format( "Collect has not Entity %s",tostring(entity)))
    -- end
    set_remove(self.entities, entity)
end

return M
