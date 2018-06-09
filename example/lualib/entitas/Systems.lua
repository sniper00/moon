local util              = require("util")
local class             = util.class
local table_insert      = table.insert
local ReactiveSystem = require("entitas.ReactiveSystem")

local M = class("Systems")

local function isinstance(a, b)
    if a.__cname == b.__cname then
        return true
    end

    while a and a.super do
        if a.super.__cname == b.__cname then
            return true
        end
        a = a.super
    end

    return false
end

function M:ctor()
    self._initialize_systems = {}
    self._execute_systems = {}
    self._net_systems = {}
    self._cleanup_systems = {}
    self._tear_down_systems = {}
end

function M:add(system)
    if system.initialize then
        table_insert(self._initialize_systems, system)
    end

    if system.execute then
        table_insert(self._execute_systems, system)
    end

    if system.cleanup then
        table_insert(self._cleanup_systems, system)
    end

    if system.tear_down then
        table_insert(self._tear_down_systems, system)
    end

    if system.dispatch then
        table_insert(self._net_systems, system)
    end
end

function M:initialize()
    for _, system in pairs(self._initialize_systems) do
        system:initialize()
    end
end

function M:execute()
    for _, system in pairs(self._execute_systems) do
        system:_execute()
    end
end

function M:dispatch(...)
    for _, system in pairs(self._net_systems) do
        system:dispatch(...)
    end
end

function M:cleanup()
    for _, system in pairs(self._cleanup_systems) do
        system:cleanup()
    end
end

function M:tear_down()
    for _, system in pairs(self._tear_down_systems) do
        system:tear_down()
    end
end

function M:activate_reactive_systems()
    for _, system in pairs(self._execute_systems) do
        if isinstance(system, ReactiveSystem) then
            system:activate()
        end

        if isinstance(system, M) then
            system:activate_reactive_systems()
        end
    end
end

function M:deactivate_reactive_systems()
    for _, system in pairs(self._execute_systems) do
        if isinstance(system, ReactiveSystem) then
            system:deactivate()
        end

        if isinstance(system, M) then
            system:deactivate_reactive_systems()
        end
    end
end

function M:clear_reactive_systems()
    for _, system in pairs(self._execute_systems) do
        if isinstance(system, ReactiveSystem) then
            system:clear()
        end

        if isinstance(system, M) then
            system:clear_reactive_systems()
        end
    end
end

return M
