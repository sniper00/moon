local moon = require("moon")

local send_failed = function(s)
    local main_service = moon.unique_service("test")
    moon.send("lua", main_service, "FAILED", moon.name(),moon.id(),s)
end

local send_success = function()
    local main_service = moon.unique_service("test")
    moon.send("lua", main_service, "SUCCESS", moon.name(),moon.id())
end

local equal = function(value, expect)
    if value ~= expect then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " got " .. tostring(value) .. filename
        send_failed(s)
    end
end

local less = function(value, expect)
    if not (value < expect) then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " < " .. tostring(value) .. filename
        send_failed(s)
    end
end

local less_equal = function(value, expect)
    if not (value <= expect) then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " <= " .. tostring(value) .. filename
        send_failed(s)
    end
end

local greater = function(value, expect)
    if not (value > expect) then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " > " .. tostring(value) .. filename
        send_failed(s)
    end
end

local greater_equal = function(value, expect)
    if not (value >= expect) then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " >= " .. tostring(value) .. filename
        send_failed(s)
    end
end

local assert = function(cond)
    if not cond then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(cond) .. filename
        send_failed(s)
    end
end

return {
    equal = equal,
    less = less,
    less_equal = less_equal,
    greater = greater,
    greater_equal = greater_equal,
    success = send_success,
    assert = assert
}
