local moon = require("moon")
local http_server = require("moon.http.server")

-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

http_server.error = function(fd, err)
    print("http server fd",fd," disconnected:",  err)
end

http_server.on("/home",function(request, response)
    -- print_r(request)
    -- print_r(request.parse_query_string(request.query_string))
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/home")
end)

http_server.listen("127.0.0.1",8001)
print("http_server start", "127.0.0.1",8001)
