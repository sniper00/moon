local moon = require("moon")

local function new_test(name)
    return setmetatable({}, { __close = function(...)
        moon.warn(...)
    end, __name = "closemeta:" .. name})
end

local i = 0
moon.dispatch("lua", function(sender, session)
    i = i + 1
    if i==2 then
        local c<close> = new_test("dispatch_error")
        error("dispatch_error")
    else
        local c<close> = new_test("dispatch_wait")
        moon.sleep(1000000)
    end
end)

moon.async(function()
    moon.sleep(10)
    local c<close> = new_test("moon.exit")
    moon.async(function()
        moon.sleep(10)
        local a<close> = new_test("stack_raise_error")
        error("raise error")
    end)
    moon.async(function()
        local a<close> = new_test("session_id_coroutine_wait")
        moon.sleep(10000000)
    end)
    moon.async(function()
        local a<close> = new_test("session_id_coroutine_call")
        moon.co_call("lua", moon.id)
    end)
    moon.async(function()
        moon.co_call("lua", moon.id)
    end)
    moon.sleep(100)
    moon.async(function()
        local a<close> = new_test("no_running")
        moon.sleep(10000000)
    end)
    moon.quit()
    moon.exit(-1)
end)