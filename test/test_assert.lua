local moon = require("moon")

local send_failed = function(s)
    local main_service = moon.queryservice("bootstrap")
    moon.send("lua", main_service, "FAILED", moon.name,moon.id,s)
end

local send_success = function()
    local main_service = moon.queryservice("bootstrap")
    moon.send("lua", main_service, "SUCCESS", moon.name,moon.id)
end

local equal = function(value, expect)
    if value ~= expect then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(expect) .. " got " .. tostring(value) .. filename
        send_failed(s)
    end
end

local linear_table_equal = function(table_value, table_expect)
    if table_value ~= table_expect then
        local eq = table.equals(table_value,table_expect)
        if eq then
            return
        end

        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. table.tostring(table_expect) .. " got " .. table.tostring(table_value) .. filename
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

local assert = function(cond, msg)
    if not cond then
        local info = debug.getinfo(2, "lS")
        local filename = string.format(" at %s(%d)", info.source, info.currentline)
        local s = "expect " .. tostring(cond) .. filename..tostring(msg)
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
    assert = assert,
    linear_table_equal = linear_table_equal
}
