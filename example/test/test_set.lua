local lu    = require('test.luaunit')
local set   = require("core.unorderset")

function test_unorderset()
    local s = set.new()
    
    for i = 1, 100 do
        set.insert(s, i)
    end
    
    assert(set.size(s) == 100)
    for i = 1, 100 do
        assert(set.has(s, i))
    end
    
    assert(set.remove(s, 11))
    assert(set.size(s) == 99)
    assert(not set.has(s, 11))
    
    for i = 1, 100 do
        if i ~= 11 then
            assert(set.has(s, i))
        end
    end
end

return function()
    local runner = lu.LuaUnit.new()
    runner:setOutputType("text")
    runner:runSuite()
end