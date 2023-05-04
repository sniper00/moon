local moon = require("moon")
local json = require("json")
local redisd = require("redisd")

local http_server = require("moon.http.server")

-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

local db_address = 0
local db_conf= {host = "127.0.0.1", port = 6379, timeout = 1000}

local HTTPSERVER_HOST = "127.0.0.1"
local HTTPSERVER_PORT = 9001

http_server.on("/redis",function(request, response)
    local query = request.parse_query()
    local cmd = string.split(query["cmd"],"-")

    local res, err = redisd.call(db_address, table.unpack(cmd))
    if not res then
        response.status_code = 400
        response:write(tostring(err))
        return
    end

    response:write_header("Content-Type", "application/json")
    response:write(json.encode(res))
end)

moon.async(function ()
    db_address = moon.new_service({
        unique = true,
        name = "db",
        file = "../service/redisd.lua",
        poolsize = 5, ---连接池
        opts = db_conf
    })

    http_server.listen(HTTPSERVER_HOST, HTTPSERVER_PORT)
    print("http_server start", HTTPSERVER_HOST..":"..HTTPSERVER_PORT)
end)

moon.shutdown(function ()
    moon.kill(db_address)
    moon.quit()
end)

-- http://127.0.0.1:9001/redis?cmd=set-hello-world
-- http://127.0.0.1:9001/redis?cmd=get-hello
-- http://127.0.0.1:9001/redis?cmd=hset-user1-name-wang
-- http://127.0.0.1:9001/redis?cmd=hset-user1-age-10
-- http://127.0.0.1:9001/redis?cmd=hget-user1-name
-- http://127.0.0.1:9001/redis?cmd=hgetall-user1

