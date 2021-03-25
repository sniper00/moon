-- define lua module search dir
local path = table.concat({
    "../lualib/?.lua",
    "../service/?.lua",
    "start_by_config/?.lua;"
},";")

package.path = path.. ";"

local moon = require("moon")
local json = require("json")

moon.set_env("PATH", string.format("package.path='%s'", package.path))

local params = json.decode(moon.get_env("PARAMS"))

local sid = math.tointeger(moon.get_env("NODE"))

local services

local switch = {}

switch[1] = function ()
    services = {
        {
            unique = true,
            name = "test",
            file = "start_by_config/test_moon.lua",
            threadid = 1
        }
    }
end

switch[2] = function ()
    services = {
        {
            unique = true,
            name = "send_benchmark_receiver1",
            file = "start_by_config/send_benchmark_receiver.lua",
        },    {
            unique = true,
            name = "send_benchmark_receiver2",
            file = "start_by_config/send_benchmark_receiver.lua",
        }
        ,
        {
            unique = true,
            name = "send_benchmark_receiver3",
            file = "start_by_config/send_benchmark_receiver.lua",
        }
        ,
        {
            unique = true,
            name = "send_benchmark_receiver4",
            file = "start_by_config/send_benchmark_receiver.lua",
        },
        {
            unique = true,
            name = "send_benchmark_sender",
            file = "start_by_config/send_benchmark_sender.lua",
        }
    }
end

switch[3] = function ()
    services = {
        {
            unique = true,
            name= "master",
            file = "start_by_config/network_text_benchmark.lua",
            host = "0.0.0.0",
            port = 42345,
            master = true,
            count = 4
        }
    }
end

switch[4] = function ()
    services = {
        {
            unique = true,
            name= "server",
            file = "start_by_config/network_benchmark.lua",
            host = "127.0.0.1",
            port = 42346,
            master = true,
            count = 4
        },
        {
            unique = true,
            name= "client",
            file = "start_by_config/network_benchmark_client.lua",
            host = "127.0.0.1",
            port = 42346,
            client_num = 1000,
            count = 100
        }
    }
end

switch[5] = function ()
    services = {
        {
            unique = true,
            name = "mysql",
            file = "start_by_config/service_mysql.lua",
            host = "127.0.0.1",
            port = 3306,
            database = "mysql",
            user="root",
            password="4321",
            timeout= 1000,
            max_packet_size=102400,
            connection_num=4
        },
        {
            unique = true,
            name = "call_mysql_service",
            file = "start_by_config/call_mysql_service.lua"
        }
    }
end

switch[6] = function ()
    services = {
        {
            unique = true,
            name = "redis",
            file = "../service/redisd.lua",
            host = "127.0.0.1",
            port = 6379,
            timeout = 1000,
            poolsize = 4
        },
        {
            unique = true,
            name = "call_redis_service",
            file = "start_by_config/call_redis_service.lua"
        }
    }
end

switch[7] = function ()
    services = {
        {
            unique = true,
            name = "sharetable",
            file = "../service/sharetable.lua"
        },
        {
            unique = true,
            name = "sharetable_example",
            file = "start_by_config/sharetable_example.lua"
        }
    }
end

switch[8] = function ()
    services = {
        {
            unique = true,
            name = "cluster",
            file = "../service/cluster.lua",
            host = params.cluster_host,
            port = params.cluster_port
        },
        {
            unique = true,
            name = "cluster_receiver",
            file = "start_by_config/cluster_receiver.lua"
        }
    }
end

switch[9] = function ()
    services = {
        {
            unique = true,
            name = "cluster",
            file = "../service/cluster.lua",
            host = params.cluster_host,
            port = params.cluster_port
        },
        {
            unique = true,
            name = "cluster_sender",
            file = "start_by_config/cluster_sender.lua"
        }
    }
end

local fn = switch[sid]
if not fn then
    return 0
end

fn()

local addrs = {}
moon.async(function()
    for _, conf in ipairs(services) do
        local service_type = conf.service_type or "lua"
        local unique = conf.unique or false
        local threadid = conf.threadid or 0
        local addr = moon.new_service(service_type, conf, unique, threadid)
        if 0 == addr then
            moon.exit(-1)
            return
        end
        table.insert(addrs, addr)
    end
end)

moon.shutdown(function()
    moon.async(function()
        for _, addr in ipairs(addrs) do
            moon.remove_service(addr)
        end
        moon.quit()
    end)
end)
