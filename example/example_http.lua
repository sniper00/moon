local moon = require("moon")
local json = require("json")
local httpc = require("moon.http.client")

local http_server = require("moon.http.server")

-- http_server.content_max_len = 8192

http_server.error = function(fd, err, network)
    if not network then
        print("http server fd", fd, " disconnected:", err)
    end
end

http_server.on("/hello", function(request, response)
    print_r(request:parse_query())
    response:write_header("Content-Type", "text/plain")
    response:write("GET:Hello World")
end)

http_server.on("/chat", function(request, response)
    print_r(request.body)
    response:write_header("Content-Type", "text/plain")
    response:write("POST:Hello World/home")
end)

http_server.on("/login", function(request, response)
    print_r(request:parse_form())
    response:write_header("Content-Type", "application/json")
    response:write(json.encode({ score = 112, level = 100, item = { id = 1, count = 2 } }))
end)

http_server.on("/login2", function(request, response)
    print_r(json.decode(request.body))
    response:write_header("Content-Type", "application/json")
    response:write(json.encode({ score = 112, level = 100, item = { id = 1, count = 2 } }))
end)

http_server.on("/fallback", function(request, response, next)
    if request.method == "GET" then
        response:write_header("Content-Type", "text/plain")
        response:write("GET:fallback")
    else
        return next()
    end
end)

http_server.fallback(function(request, response, next)
    if request.path == "/end" then
        return next()
    end
    response:write_header("Content-Type", "text/plain")
    response:write("echo:" .. request.path)
end)

http_server.fallback(function(request, response, next)
    error("end")
end)

http_server.listen("127.0.0.1", 9991)
print("http_server start", "127.0.0.1", 9991)

if false then -- Set true to test http proxy
    -- Set http proxy

    -- Solution 1: Set the environment variable HTTPS_PROXY
    moon.env("HTTPS_PROXY", "http://127.0.0.1:8443")
    moon.async(function ()
        print_r(httpc.get("https://www.google.com.hk"))
    end)

    -- Solution 2: Set the proxy parameter
    --proxy = "http://127.0.0.1:8443"
    print_r(httpc.get("https://www.google.com.hk", {
        proxy = "http://127.0.0.1:8443"
    }))
end

moon.async(function()
    print_r(httpc.get("http://127.0.0.1:9991/hello?a=1&b=2"))

    print_r(httpc.post("http://127.0.0.1:9991/chat", "Hello Post"))

    local form = { username = "wang", passwd = "456", age = 110 }
    local response = httpc.post_form("http://127.0.0.1:9991/login", form)
    print_r(response.body)
    http_server.off("/login")
    local response = httpc.post_form("http://127.0.0.1:9991/login", form)
    print_r(response.body)

    local response = httpc.post_json("http://127.0.0.1:9991/login2", { username = "wang", passwd = "456", age = 110 })
    print_r(response.body)

    local response = httpc.get("http://127.0.0.1:9991/fallback")
    print_r(response.body)
    local response = httpc.post("http://127.0.0.1:9991/fallback", "")
    print_r(response.body)
    local response = httpc.get("http://127.0.0.1:9991/random")
    print_r(response.body)
    local response = httpc.get("http://127.0.0.1:9991/end")
    print_r(response.body)

    moon.exit(100)
end)
