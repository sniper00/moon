local set           = require("unorderset")
local GroupEvent    = require("entitas.GroupEvent")
local set_insert    = set.insert

local M  = {}
M.__index = M

function M.new(groups)
    local tb = {}
    tb.entities = set.new()
    tb._groups = groups
    tb.add_entity = function(...) return tb._add_entity(tb, ...) end
    tb = setmetatable(tb, M)
    tb.activate(tb)
    return tb
end

--Activates the Collector and will start collecting
--changed entities. Collectors are activated by default.
function M:activate()
    for group, group_event in pairs(self._groups) do
        local added_event = group_event == GroupEvent.ADDED
        local removed_event = group_event == GroupEvent.REMOVED
        local added_or_removed_event = group_event == GroupEvent.ADDED_OR_REMOVED

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
    set_insert(self.entities, entity)
end

return M
