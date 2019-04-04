local moon = require("moon")
local redis = require("moon.db.redis")

local tbremove = table.remove
local tbinsert = table.insert
local setmetatable = setmetatable

local M = {
    VERSION = "0.1",
    ip = "127.0.0.1",
    port = "6379"
}

local mt = {__index = M}

function M.new(ip, port, max_size)
    local t = {}
    t.pool = {}
    t.max_size = max_size or 10
    t._size = 0
    if ip and port then
        t.ip = ip
        t.port = port
    end
    return setmetatable(t, mt)
end

function M:spawn(trytimes)
    local c = tbremove(self.pool)
    if not c then
        c = redis.new()
        if not c:async_connect(self.ip, self.port) then
            return nil, "connect redis failed"
        end
        self._size = self._size + 1
    else
        local ok, _ = c:docmd("PING")
        if not ok then
            print("span redis not connect,reconnecting...")
            while not c:async_connect(self.ip, self.port) do
                print("reconnect redis server failed")
                if trytimes then
                    trytimes = trytimes - 1
                    if trytimes <= 0 then
                        return nil, "connect redis failed"
                    end
                end
                moon.co_wait(1000)
            end
        end
    end
    return c
end

function M:close(c)
    if self._size >= self.max_size then
        local oc = tbremove(self.pool)
        if oc then
            oc:close()
            self._size = self._size - 1
        end
    end
    tbinsert(self.pool, c)
end

function M:size()
    return self._size
end

return M
