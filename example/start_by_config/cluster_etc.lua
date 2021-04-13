local moon = require("moon")
local json = require("json")
local httpserver = require("moon.http.server")

local conf = ...

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

