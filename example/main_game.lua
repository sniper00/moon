local socket = require "moon.socket"
---__init__
if _G["__init__"] then
    local arg = ...
    return {
        thread = 16,
        enable_stdout = true,
        logfile = string.format("log/moon-%s-%s.log", arg[1], os.date("%Y-%m-%d-%H-%M-%S")),
        loglevel = "DEBUG",
    }
end

-- custom lua module search dir
local path = table.concat({
    "../lualib/?.lua",
    "../service/?.lua",
},";")

package.path = path.. ";"

local moon = require("moon")

moon.env("PATH", string.format("package.path='%s'", package.path))

local arg = moon.args()

local sid = math.tointeger(arg[1])

local params = {
    gate = {host = "127.0.0.1", port = 12345},
    db = {
        redis = { host = "127.0.0.1", port = 6379, db = 0, timeout = 1000},
        pg =  { user = "postgres", password = "123456", host = "127.0.0.1", port = 5432, connect_timeout = 1000, database = "postgres" }
    }
}

local services = {
    -- {
    --     unique = true,
    --     name = "mysql",
    --     file = "../service/sqldriver.lua",
    --     provider = "moon.db.mysql",
    --     opts = params.mysql,
    --     poolsize = 5,
    --     threadid = 1
    -- },
    -- {
    --     unique = true,
    --     name = "game_db",
    --     file = "../service/sqldriver.lua",
    --     provider = "moon.db.pg",
    --     opts = params.db.pg,
    --     poolsize = 5,
    --     threadid = 2
    -- },
    -- {
    --     unique = true,
    --     name = "game_redis",
    --     file = "../service/redisd.lua",
    --     opts = params.db.redis,
    --     poolsize = 5,
    --     threadid = 3
    -- },
    {
        unique = true,
        name = "sharetable",
        file = "../service/sharetable.lua",
        dir = "table",
        threadid = 4
    },
    {
        unique = true,
        name = "gate",
        file = "game/service_gate.lua",
        threadid = 5,
        host = params.gate.host,
        port = params.gate.port
    },
}

for i=1,10 do
    table.insert(services, {
        unique = true,
        name = "sence"..i,
        file = "game/service_sence.lua",
    })
end

local addrs = {}
moon.async(function()
    local fd = socket.connect(params.db.pg.host, params.db.pg.port, moon.PTYPE_SOCKET_TCP, 1000)
    if fd then
        socket.close(fd)
        table.insert(services,{
            unique = true,
            name = "game_db",
            file = "../service/sqldriver.lua",
            provider = "moon.db.pg",
            opts = params.db.pg,
            poolsize = 5,
            threadid = 2
        })
    end

    fd = socket.connect(params.db.redis.host, params.db.redis.port, moon.PTYPE_SOCKET_TCP, 1000)
    if fd then
        socket.close(fd)
        table.insert(services,{
            unique = true,
            name = "game_redis",
            file = "../service/redisd.lua",
            opts = params.db.redis,
            poolsize = 5,
            threadid = 3
        })
    end

    for _, conf in ipairs(services) do
        local service_type = conf.service_type or "lua"
        local addr = moon.new_service(service_type, conf)
        if 0 == addr then
            moon.exit(-1)
            return
        end
        table.insert(addrs, addr)
    end

    moon.sleep(5000)

    moon.exit(0)
end)

moon.shutdown(function()
    moon.async(function()
        for _, addr in ipairs(addrs) do
            moon.kill(addr)
        end
        moon.quit()
    end)
end)
