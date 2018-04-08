local util              = require("util")
local class             = util.class
local table_insert      = table.insert
local Collector         = require("entitas.Collector")

local ReactiveProcessor = class("ReactiveProcessor")

local function get_collector(self,context)
    local trigger = self:get_trigger()
    local collector = Collector.new()

    for matcher, group_event in pairs(trigger) do
        local group = context:get_group(matcher)
        collector:add(group, group_event)
    end

    return collector
end

function ReactiveProcessor:ctor(context)
    self._collector = get_collector(self,context)
    self._buffer = {}
end

function ReactiveProcessor:get_trigger()
    error("not imp")
end

function ReactiveProcessor:react()
    error("not imp")
end

function ReactiveProcessor:activate()
    self._collector:activate()
end

function ReactiveProcessor:deactivate()
    self._collector:deactivate()
end

function ReactiveProcessor:clear()
    self._collector:clear_collected_entities()
end

function ReactiveProcessor:execute()
    if self._collector.collected_entities then
        for _, entity in pairs(self._collector.collected_entities) do
            if self:filter(entity) then
                table_insert(self._buffer, entity)
            end
        end

        self._collector:clear_collected_entities()

        if #self._buffer > 0 then
            self:react(self._buffer)
            self._buffer = {}
        end
    end
end

return ReactiveProcessor
