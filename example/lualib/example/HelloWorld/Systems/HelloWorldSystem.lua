local util       = require("util")
local class      = util.class
local Components = require('example.HelloWorld.Components')
local DebugMessageComponent = Components.DebugMessage

local HelloWorldSystem = class("HelloWorldSystem")

function HelloWorldSystem:ctor(context)
    self.context = context
end

function HelloWorldSystem:initialize()
    --create an entity and give it a DebugMessageComponent with
    --the text "Hello World!" as its data
    local entity = self.context:create_entity()
    entity:add(DebugMessageComponent,"HelloWorld")
end


return HelloWorldSystem