local moon = require("moon")
local http_server = require("moon.http.server")
local httpc = require("moon.http.client")
local json = require("json")
local test_assert = require("test_assert")
-- http_server.header_max_len = 8192
-- http_server.content_max_len = 8192

http_server.error = function(fd, err)
    print("http server fd",fd," disconnected:",  err)
end

http_server.on("/home",function(request, response)
    -- print_r(request)
    -- print_r(request.parse_query_string(request.query_string))
    test_assert.equal(request.body,"HAHAHA")
    response:write_header("Content-Type","text/plain")
    response:write("Hello World/home")
end)

http_server.on("/query", function(request, response)
    local query = request:parse_query()
    test_assert.equal(request.method, "GET")
    test_assert.equal(query.name, "moon")
    test_assert.equal(query.lang, "lua")
    response:write_header("Content-Type", "text/plain")
    response:write(query.name .. ":" .. query.lang)
end)

http_server.on("/form", function(request, response)
    local form = request:parse_form()
    test_assert.equal(form.a, "1")
    test_assert.equal(form.b, "hello world")
    response:write_header("Content-Type", "text/plain")
    response:write("FORM_OK")
end)

http_server.on("/json", function(request, response)
    local data = json.decode(request.body)
    test_assert.equal(data.name, "moon")
    test_assert.equal(data.id, 42)
    response:write_header("Content-Type", "application/json")
    response:write(json.encode({ ok = true, echoed = data.name }))
end)

http_server.listen("127.0.0.1",8001)

moon.async(function()
    local response = httpc.post("http://127.0.0.1:8001/home","HAHAHA")
    -- print_r(response)
    test_assert.equal(response.body,"Hello World/home")

    response = httpc.get("http://127.0.0.1:8001/query?name=moon&lang=lua")
    test_assert.equal(response.status_code, 200)
    test_assert.equal(response.body, "moon:lua")

    response = httpc.post_form("http://127.0.0.1:8001/form", { a = 1, b = "hello world" })
    test_assert.equal(response.status_code, 200)
    test_assert.equal(response.body, "FORM_OK")

    response = httpc.post_json("http://127.0.0.1:8001/json", { name = "moon", id = 42 })
    test_assert.equal(response.status_code, 200)
    test_assert.equal(response.body.ok, true)
    test_assert.equal(response.body.echoed, "moon")

    response = httpc.get("http://127.0.0.1:8001/not-found")
    test_assert.equal(response.status_code, 404)
    test_assert.assert(type(response.body) == "string" and response.body:find("Cannot GET /not%-found") ~= nil, "404 body should mention missing path")

    local query = httpc.create_query_string({ a = "1", b = "hello world" })
    test_assert.assert(type(query) == "string" and #query > 0, "create_query_string should return non-empty string")

    test_assert.success()
end)
