--  Bring your systems together
local Context = require('entitas.Context')
local Processors = require('entitas.Processors')
local HelloWorldSystem = require('example.HelloWorld.Systems.HelloWorldSystem')
local DebugMessageSystem = require('example.HelloWorld.Systems.DebugMessageSystem')
local CleanupDebugMessageSystem = require('example.HelloWorld.Systems.CleanupDebugMessageSystem')
local Components = require('example.HelloWorld.Components')
local Matcher = require('entitas.Matcher')
local DebugMessageComponent = Components.DebugMessage
local _context = Context.new()

local processors = Processors.new()
processors:add(HelloWorldSystem.new(_context))
processors:add(DebugMessageSystem.new(_context))
processors:add(CleanupDebugMessageSystem.new(_context))

processors:activate_reactive_processors()
processors:initialize()

local _group = _context:get_group(Matcher({DebugMessageComponent}))
assert(_group.entities:size() == 1)
processors:execute()
processors:cleanup()
assert(_group.entities:size() == 0)

processors:clear_reactive_processors()
processors:tear_down()