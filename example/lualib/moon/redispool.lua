local redis = require("moon.redis")
local moon = require("moon")

local M = {
    VERSION = '0.1',
    ip = "127.0.0.1",
    port = "6379"
}

local mt = {__index = M}

function M.new( ip, port )
    local t = {}
    t.pool = {}
    if ip and port then
        t.ip = ip
        t.port = port
    end
    return setmetatable(t, mt)
end

function M:spawn()
    local c = table.remove(self.pool)
    if not c then
        c = redis.new()
        if not c:connect(M.ip, M.port) then
            return nil,"connect redis failed"
        end
        return c
    else
        local ok,_ = c:docmd("PING")
        if not ok then
            print("span redis not connect,reconnecting...")
            while not c:async_connect(self.ip, self.port) do
                print("reconnect redis server failed")
                moon.co_wait(1000)
            end
        end
        return c
    end
end

function M:close(c)
    table.insert(self.pool,c)
end

function M:size()
    return #self.pool
end

return M