local set           = require("unorderset")
local GroupEvent    = require("entitas.GroupEvent")
local set_insert    = set.insert

local Collector  = {}
Collector.__index = Collector

function Collector.new()

    local tb = {}
    tb.collected_entities = set.new()
    tb._groups = {}
    tb.add_entity = function(...) return tb._add_entity(tb, ...) end
    return setmetatable(tb, Collector)
end

function Collector:add(group, group_event)
    self._groups[group] = group_event
end

function Collector:activate()
    for group, group_event in pairs(self._groups) do
        local added_event = group_event == GroupEvent.added
        local removed_event = group_event == GroupEvent.removed
        local added_or_removed_event = group_event == GroupEvent.added_or_removed

        if added_event or added_or_removed_event then
            group.on_entity_added:remove(self.add_entity)
            group.on_entity_added:add(self.add_entity)
        end

        if removed_event or added_or_removed_event then
            group.on_entity_removed:remove(self.add_entity)
            group.on_entity_removed:add(self.add_entity)
        end
    end
end

function Collector:deactivate()
    for group, _ in pairs(self._groups) do
        group.on_entity_added:remove(self.add_entity)
        group.on_entity_removed:remove(self.add_entity)
    end

    self:clear_collected_entities()
end

function Collector:clear_collected_entities()
    self.collected_entities = set.new()
end

function Collector:_add_entity(entity)
    set_insert(self.collected_entities, entity)
end

return Collector
