local moon = require("moon")
local http_server = require("moon.net.http_server")
local http_client = require("moon.net.http_client")
local test_assert = require("test_assert")
-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

http_server.error = function(session, err)
    print("http server session",session.connid," disconnected:",  err)
end

http_server.on("/home",function(request, response, data)
    -- print("SERVER: http_version",request.http_version)
    -- print("SERVER: query_string",request.query_string)
    -- print("SERVER: Content-Type",request:header("Content-Type"))
    -- print("SERVER: data",data)
    test_assert.equal(data,"HAHAHA")
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/home")
end)

http_server.listen("127.0.0.1",8001)


moon.start(function (  )
    moon.async(function ()
        local client = http_client.new("127.0.0.1:8001")
        local header,data = client:request("GET","/home","HAHAHA")
        -- print("CLIENT: http_version",header.version)
        -- print("CLIENT: status_code",header.status_code)
        -- print("CLIENT: data",data)
        test_assert.equal(data,"Hello World/home")
        test_assert.success()
    end)
end)