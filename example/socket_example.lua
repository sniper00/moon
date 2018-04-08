local moon = require("moon")
local socket = require("moon.socket")
local seri = require("seri")

local function test_read_line(session)
    moon.start_coroutine(function()
        while true do
            local data, err = session:co_readline()
            if not data then
                print(session.connid, err)
                return
            else
                if data == 'exit' then
                    print("exit")
                    assert(session:close())
                    return
                else
                    session:send(data)
                end
            end
        end
    end)
end

local function test_http(session)
    moon.start_coroutine(function()
        local http_request = moon.http_request.new()
        while true do
            local data, err = session:co_read('\r\n\r\n')
            if not data then
                print(session.connid, err)
                return
            else
                local n = http_request:parse(data)
                if n == -1 then
                    print("parse http header failed")
                    return
                end

                print(http_request.query_string)

                local response = {
                    'HTTP/1.1 200 OK\r\n',
                    'Content-Length: 10\r\n',
                    'Content-Type: text/plain;charset=utf-8\r\n',
                    '\r\n',
                    'HelloWorld'
                }
                session:send(seri.concat(response))
            end
        end
    end)
end

moon.init(function( config )
    local server =  socket.new()

    server:settimeout(10)

    server:listen(config.ip,config.port)

    moon.start_coroutine(function()
        while true do
            local session = server:co_accept()
            if session then
                test_http(session)
            end
        end
    end)
    return true
end)




