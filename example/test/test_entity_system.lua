local lu = require('test.luaunit')
local moon = require('moon')

local Entity = require('entitas.Entity')
local Context = require('entitas.Context')
local Matcher = require('entitas.Matcher')
local Collector = require('entitas.Collector')
local MakeComponent = require('entitas.MakeComponent')
local GroupEvent = require("entitas.GroupEvent")
local EntityIndex = require('entitas.EntityIndex')
local PrimaryEntityIndex = require('entitas.PrimaryEntityIndex')
local Processors = require('entitas.Processors')

local Position = MakeComponent("Position", "x", "y", "z")
local Movable = MakeComponent("Movable", "speed")
local Person = MakeComponent("Person", "name","age")
local Counter = MakeComponent("Counter", "num")
local PlayerData = MakeComponent("PlayerData", "name")

local GLOBAL = moon.exports

GLOBAL.test_collector =  function()
    local context = Context.new()
    local group = context:get_group(Matcher({Position}))
    local collector = Collector.new()
    collector:add(group, GroupEvent.added)
    collector:clear_collected_entities()
    collector:deactivate()
end

GLOBAL.test_context =  function()
    local _context = Context.new()
    local _entity = _context:create_entity()

    assert(Context.create_entity)
    assert(Context.has_entity)
    assert(Context.destroy_entity)
    assert(Context.get_group)
    assert(Context.set_unique_component)
    assert(Context.get_unique_component)

    lu.assertEquals(_context:has_entity(_entity), true)
    lu.assertEquals(_context:entity_size(), 1)
    _context:destroy_entity(_entity)
    assert(not _context:has_entity(_entity))

    -- reuse
    local _e2 = _context:create_entity()
    lu.assertEquals(_context:has_entity(_entity), true)
    lu.assertEquals(_context:entity_size(), 1)


    _context:set_unique_component(Counter,101)
    local cmp = _context:get_unique_component(Counter)
    assert(cmp.num == 101)
end

GLOBAL.test_index =  function()
    local context = Context.new()
    local group = context:get_group(Matcher({Person}))

    --print("group", group)

    local index = EntityIndex.new(Person, group, 'age')
    context:add_entity_index(index)
    local adam = context:create_entity()
    adam:add(Person, 'Adam', 42)
    local eve = context:create_entity()
    eve:add(Person, 'Eve', 42)

    local idx = context:get_entity_index(Person)
    local entities = idx:get_entities(42)

    assert(entities:has(adam))
    assert(entities:has(eve))
end

GLOBAL.test_primary_index =  function()

    local ett = Entity.new()
    local context = Context.new()
    local group = context:get_group(Matcher({Person}))
    group = context:get_group(Matcher({Person}))
    group = context:get_group(Matcher({Person}))

    local primary_index = PrimaryEntityIndex.new(Person, group, 'name')
    context:add_entity_index(primary_index)

    local adam = context:create_entity()
    adam:add(Person, 'Adam', 42)

    local eve = context:create_entity()
    eve:add(Person, 'Eve', 42)

    local idx = context:get_entity_index(Person)
    local ety = idx:get_entity("Eve")
    assert(primary_index == idx)
    assert(ety == eve)
end

GLOBAL.test_entity =  function()

    local entity = Entity.new()

    entity:activate(0)
    entity:add(Position, 1, 4, 5)
    assert(entity:has(Position))
    assert(entity:has_any(Position))

    local pos = entity:get(Position)
    assert(pos.x == 1)
    assert(pos.y == 4)
    assert(pos.z == 5)

    entity:replace(Position, 5, 6)

    entity:replace(Person, "wang")

    assert(entity:get(Position).x == 5)
    assert(entity:get(Position).y == 6)

    entity:remove(Position)
    assert(not entity:has(Position))

    entity:add(Position, 1, 4, 5)
    entity:add(Movable, 0.56)
    assert(entity:has(Position, Movable))
    entity:destory()
    assert(not entity:has(Position, Movable))
end

GLOBAL.test_group =  function()

    local _context = Context.new()
    local _entity = _context:create_entity()

    _entity:add(Movable, 1)

    local _group = _context:get_group(Matcher({Movable}))

    assert(_group.entities:size() == 1)
    assert(_group:single_entity():has(Movable))

    assert(_group:single_entity() == _entity)
    _entity:replace(Movable, 2)
    assert(_group:single_entity() == _entity)
    _entity:remove(Movable)
    assert(not _group:single_entity())

    _entity:add(Movable, 3)

    local _entity2 = _context:create_entity()
    _entity2:add(Movable, 10)
    lu.assertEquals(_group.entities:size(), 2)
    local entities = _group.entities

    assert(entities:has(_entity))
    assert(entities:has(_entity2))
end

GLOBAL.test_matches =  function()
    local CompA = MakeComponent('CompA', '')
    local CompB = MakeComponent('CompB', '')
    local CompC = MakeComponent('CompC', '')
    local CompD = MakeComponent('CompD', '')
    local CompE = MakeComponent('CompE', '')
    local CompF = MakeComponent('CompF', '')

    local ea = Entity.new()
    local eb = Entity.new()
    local ec = Entity.new()
    ea:activate(0)
    eb:activate(1)
    ec:activate(2)
    ea:add(CompA)
    ea:add(CompB)
    ea:add(CompC)
    ea:add(CompE)
    eb:add(CompA)
    eb:add(CompB)
    eb:add(CompC)
    eb:add(CompE)
    eb:add(CompF)
    ec:add(CompB)
    ec:add(CompC)
    ec:add(CompD)

    local matcher = Matcher(
        {CompA, CompB, CompC},
        {CompD, CompE},
        {CompF}
    )
    assert(matcher:matches(ea))
    assert(not matcher:matches(eb))
    assert(not matcher:matches(ec))
end

GLOBAL.test_10000_entities = function()

    local _context = Context.new()

    for i=1,10000 do
        local _entity = _context:create_entity()
        _entity:add(Movable,i)
        _entity:add(PlayerData,i)
    end

    local _group = _context:get_group(Matcher({Movable}))

    assert(_group.entities:size() == 10000)

    local index = EntityIndex.new(PlayerData, _group, 'name')
    _context:add_entity_index(index)

    local idx = _context:get_entity_index(PlayerData)
    local entities = idx:get_entities(100)
    assert(entities:size() == 1)
end


local StartGame = {
        initialize = function() print("StartGame initialize") end
    }

local MoveSystem = {
        execute = function() print("MoveSystem execute") end
    }

local EndSystem = {
        tear_down = function() print("EndSystem tear_down") end
    }



GLOBAL.test_system = function()

    local _context = Context.new()

    local processors = Processors.new()
    processors:add(StartGame)
    processors:add(MoveSystem)
    processors:add(EndSystem)

    processors:initialize()

    processors:execute()

    processors:tear_down()
end

return function()
    local runner = lu.LuaUnit.new()
    runner:setOutputType("tap")
    local ret = runner:runSuite()
    if 0 == ret then
        print("test_entity_system success with result "..ret)
    else
        print("test_entity_system failed with result "..ret)
    end
end
