local util       = require("util")
local class      = util.class
local ReactiveProcessor = require("entitas.ReactiveProcessor")
local Matcher = require('entitas.Matcher')
local Components = require('example.HelloWorld.Components')
local GroupEvent = require("entitas.GroupEvent")

local DebugMessageComponent = Components.DebugMessage

local DebugMessageSystem = class("DebugMessageSystem",ReactiveProcessor)

function DebugMessageSystem:ctor(context)
    self.context = context
    DebugMessageSystem.super.ctor(self,context)
end

function DebugMessageSystem:get_trigger()
    --we only care about entities with DebugMessageComponent
    return {Matcher({DebugMessageComponent}),GroupEvent.ADDED}
end

function DebugMessageSystem:filter(entity)
    --good practice to perform a final check in case 
    --the entity has been altered in a different system.
    return entity:has(DebugMessageComponent)
end

function DebugMessageSystem:react(entites)
    for k,entity in pairs(entites) do
        print(entity:get(DebugMessageComponent).message)
    end
end

return DebugMessageSystem
