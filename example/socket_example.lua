local moon = require("moon")
local http_server = require("moon.http_server")
local http_client = require("moon.http_client")

http_server.error = function(session, err)
    print("http server session",session.connid," disconnected:",  err)
end

http_server.on("/",function(request, response, data)
    print("SERVER: http_version",request.http_version)
    print("SERVER: query_string",request.query_string)
    print("SERVER: Content-Type",request:header("Content-Type"))
    print("SERVER: data",data)
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/")
end)

http_server.on("/home",function(request, response, data)
    print("SERVER: http_version",request.http_version)
    print("SERVER: query_string",request.query_string)
    print("SERVER: Content-Type",request:header("Content-Type"))
    print("SERVER: data",data)
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/home")
end)

http_server.listen("127.0.0.1",8001)


moon.start(function (  )
    moon.async(function ()
        local client = http_client.new("127.0.0.1:8001")
        while true do
            local header,data = client:request("GET","/home","HAHAHA")
            if not header then
                print("error",data)
                return
            end
            print("CLIENT: http_version",header.version)
            print("CLIENT: status_code",header.status_code)
            print("CLIENT: data",data)
            moon.co_wait(1000)
        end
    end)
end)