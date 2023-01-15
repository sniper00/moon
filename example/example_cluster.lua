-- define lua module search dir
local path = table.concat({
    "../lualib/?.lua",
    "../service/?.lua",
},";")

package.path = path.. ";"


local moon = require("moon")

moon.env("PATH", string.format("package.path='%s'", package.path))

local conf = ...

local function run_cluster_etc()
    local json = require("json")
    local httpserver = require("moon.http.server")

    httpserver.content_max_len = 8192
    httpserver.header_max_len = 8192

    httpserver.error = function(fd, err)
        moon.warn(fd," disconnected:",  err)
    end

    local cluster_etc

    local function load_cluster_etc()
        cluster_etc = {}
        local res = json.decode(io.readfile(conf.config))
        for _,v in ipairs(res) do
            cluster_etc[v.node] = v
        end
    end

    load_cluster_etc()

    httpserver.on("/reload",function(request, response)
        load_cluster_etc()
        response.status_code = 200
        response:write_header("Content-Type","text/plain")
        response:write("OK")
    end)

    httpserver.on("/cluster",function(request, response)
        local query = request.parse_query()
        local node = tonumber(query.node)
        local cfg = cluster_etc[node]
        if not cfg or not cfg.host or not cfg.port  then
            response.status_code = 404
            response:write_header("Content-Type","text/plain")
            response:write("cluster node not found "..tostring(query.node))
            return
        end
        response.status_code = 200
        response:write_header("Content-Type","application/json")
        response:write(json.encode({host = cfg.host, port = cfg.port}))
    end)

    httpserver.listen(conf.host, conf.port, conf.timeout)
    print("Cluster etc http server start", conf.host, conf.port)

    moon.shutdown(function()
        moon.quit()
    end)
end


local function run_cluster_receiver()

    local command = {}

    command.ADD =  function(a,b)
        return a+b
    end

    command.SUB = function(a,b)
        return a-b
    end

    command.MUL = function(a,b)
        return a*b
    end

    command.ACCUM = function(...)
        local numbers = {...}
        local total = 0
        for _,v in pairs(numbers) do
            total = total + v
        end
        return total
    end

    local count = 0
    local tcount = 0
    local bt = 0
    command.COUNTER = function(t)
        -- print(...)
        count = count + 1
        if bt == 0 then
            bt = t
        end
        tcount = tcount + 1

        if count == 10000 then
            print("per", (t-bt))
            count = 0
            bt = 0
        end
    end

    -- moon.async(function()
    --     while true do
    --         moon.sleep(1000)
    --         print(tcount)
    --     end
    -- end)


    moon.dispatch('lua',function(sender, session, CMD, ...)
        local f = command[CMD]
        if f then
            if CMD ~= 'ADD' then
                --moon.sleep(20000)
                moon.response('lua',sender, session,f(...))
            end
        else
            error(string.format("Unknown command %s", tostring(CMD)))
        end
    end)
end

local function run_cluster_sender()

    local cluster = require("cluster")

    local counter = 0

    moon.async(function()
        while true do
            moon.sleep(1000)
            print("per sec ",counter)
            counter=0
        end
    end)

    local args = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
    moon.async(function()

        print(cluster.call(1, 'cluster_receiver', "ACCUM", table.unpack(args)))
        for i=1,100000 do
            cluster.send(1, 'cluster_receiver',"COUNTER", moon.now())
        end

        while true do
            local ret ,err = cluster.call(1, 'cluster_receiver', "ACCUM", table.unpack(args))
            if not ret then
                print(err)
                return
            end
            counter=counter+1
        end
    end)

end

if conf.name == "cluster_etc" then
    run_cluster_etc()
    return
elseif conf.name == 'cluster_receiver' then
    run_cluster_receiver()
    return
elseif conf.name == 'cluster_sender' then
    run_cluster_sender()
    return
end

local services = {
    {
        unique = true,
        name = "cluster_etc",
        file = "example_cluster.lua",
        host = "127.0.0.1",
        port = 40001,
        config = "cluster_node.json"
    },
    {
        unique = true,
        name = "cluster",
        file = "../service/cluster.lua",
        host = "127.0.0.1",
        port = 42345,
        etc_host = "127.0.0.1:40001",
        etc_path = "/cluster"
    },
    {
        unique = true,
        name = "cluster_receiver",
        file = "example_cluster.lua"
    },
    {
        unique = true,
        name = "cluster_sender",
        file = "example_cluster.lua"
    }
}

local addrs = {}
moon.async(function ()

    moon.env("NODE",  "1")

    for _, cfg in ipairs(services) do
        local service_type = conf.service_type or "lua"
        local addr = moon.new_service(service_type, cfg)
        if 0 == addr then
            moon.exit(-1)
            return
        end
        table.insert(addrs, addr)

        if cfg.name == "cluster" then
            print(moon.co_call("lua", moon.queryservice("cluster"), "Start"))
        end
    end
end)

moon.shutdown(function()
    moon.async(function()
        for _, addr in ipairs(addrs) do
            moon.kill(addr)
        end
        moon.quit()
    end)
end)