local moon = require("moon")
local test_assert = require("test_assert")
local reload = require "hotfix"
local require = reload.require
-- reload.postfix = "_update"	-- for test

---hotfix example
local source = {}

source["old"] = [[
    local moon = require("moon")

    local context = ...

    ---Upvalues(not function type) will Reserved
    local a = 100
    local b = 200

    local switch = {}

    switch[1] = function()
        moon.warn("f1", b)
    end

    local M = {}

    M.__index = M

    function M.new()
        local obj = {
            n = 0,
            m = "hello"
        }
        return setmetatable(obj,M)
    end

    ---Function type upvalue will replaced by new version
    local function tmp(self)
        switch[1]()
        self.n=self.n+1000
    end

    ---Function fields will replaced by new version
    function M:func()
        tmp(self)
        print("before", self.n, self.m, a-b)
        moon.sleep(1000)
        return a - b
    end

    return M
]]

source["new"] = [[
    local moon = require("moon")

    --- Hotfix only loads a lua chunk without running it.
    --- Your initialize code will not run again.
    assert(false)

    local a = 1
    local b = 2

    local switch = {}

    switch[1] = function()
        moon.warn("f1", b)
    end

    local M = {}

    M.__index = M

    function M.new()
        local obj = {
            n = 0,
            m = "world"
        }
        return setmetatable(obj,M)
    end

    local function tmp(self)
        switch[1]()
        self.n=self.n-999
    end

    function M:func()
        tmp(self)
        print("after", self.n, self.m, a + b)
        moon.sleep(1000)
        return a + b
    end

    return M
]]

reload.addsearcher(function(name)
    local content = source[name]
    return load(content,"@"..name),name
end)

local reload_module = require("old")

moon.async(function ()
    local rmd = reload_module.new()
    for k,v in pairs(reload_module) do
        print(k,v)
    end
    assert(rmd:func() == -100) --- output: before    1000    hello   -100
    print("Hot Fix Result",reload.update("old", "new"))
    assert(rmd:func() == 300) --- outpur: after     1       hello   300
    test_assert.success()
end)


