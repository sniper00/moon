local moon = require("moon")
local socket = require("moon.socket")
local seri = require("seri")

local function read_line_example(session)
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

local function http_example(session)
    moon.start_coroutine(function()
        local http_request = moon.http_request.new()
        while true do
            local data, err = session:co_read('\r\n\r\n')
            if not data then
                --print(session.connid, err)
                return
            else
                --print(data)
                local n = http_request:parse(data)
                if n == -1 then
                    --print("parse http header failed")
                    return
                end

                --print(http_request.query_string)

                local da = os.date("!Date: %a, %d %b %Y %X GMT\r\n",os.time())

                local conn_type = http_request:get_header("Connection")
                if not conn_type or 0== #conn_type then
                    conn_type = 'close'
                end

                local response = {
                    'HTTP/1.1 200 OK\r\n',
                    'Content-Type: text/plain\r\n',
                    'Content-Length: 10\r\n',
                    da,
                    'Connection: '..conn_type..'\r\n',
                    '\r\n',
                    'Helloworld'
                }
                --print(seri.concatstring(response))
                if conn_type == 'close' then
                    --print("will close")
                    session:send(seri.concat(response),true)
                else
                    session:send(seri.concat(response))
                end
            end
        end
    end)
end

moon.init(function( config )
    local server =  socket.new()

    --server:settimeout(10)

    server:listen(config.ip,config.port)

    moon.start_coroutine(function()
        while true do
            local session = server:co_accept()
            if session then
                http_example(session)
            end
        end
    end)
    return true
end)




