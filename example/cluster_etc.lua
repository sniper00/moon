local moon = require("moon")
local socket = require("moon.socket")
local json = require("json")
local httpserver = require("moon.http.server")

local node_file = moon.args()[1]

local ETC_HOST = "127.0.0.1"
local ETC_PORT = 9090

httpserver.content_max_len = 8192
httpserver.header_max_len = 8192

httpserver.error = function(fd, err)
    moon.warn(fd," disconnected:",  err)
end

local cluster_etc

local function load_cluster_etc()
    cluster_etc = {}
    local res = json.decode(io.readfile(node_file))
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

--- 通过node id查询 cluster 端口配置
httpserver.on("/cluster",function(request, response)
    local query = request:parse_query()
    local node = tonumber(query.node)
    local cfg = cluster_etc[node]
    if not cfg then
        response.status_code = 404
        response:write_header("Content-Type","text/plain")
        response:write("cluster node not found "..tostring(query.node))
        return
    end
    response.status_code = 200
    response:write_header("Content-Type","application/json")

    local host, port = socket.parse_host_port(cfg.cluster)
    response:write(json.encode({host = host, port = port}))
end)

--- 通过node id查询节点配置
httpserver.on("/conf.node",function(request, response)
    local query = request:parse_query()
    local node = tonumber(query.node)
    local cfg = cluster_etc[node]
    if not cfg then
        response.status_code = 404
        response:write_header("Content-Type","text/plain")
        response:write("cluster node not found "..tostring(query.node))
        return
    end
    response.status_code = 200
    response:write_header("Content-Type","application/json")
    response:write(json.encode(cfg))
end)

httpserver.listen(ETC_HOST, ETC_PORT, 60)
print("Cluster etc http server start", ETC_HOST, ETC_PORT)
