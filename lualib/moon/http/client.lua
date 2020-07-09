local http = require("http")
local seri = require("seri")
local moon = require("moon")
local socket = require("moon.socket")

local tbinsert = table.insert
local tbconcat = table.concat

local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local assert = assert
local error = error

local parse_response = http.parse_response
local create_query_string = http.create_query_string

-----------------------------------------------------------------

local function read_chunked(fd)

    local chunkdata = {}

    while true do
        local data, err = socket.readline(fd, "\r\n")
        assert(data,err)
        local length = tonumber(data,"16")
        if not length then
            error("Invalid response body")
        end

        if length==0 then
            break
        end

        if length >0 then
            data, err = socket.read(fd, length)
            assert(data,err)
            tbinsert( chunkdata, data )
            data, err = socket.readline(fd, "\r\n")
            assert(data,err)
        elseif length <0 then
            error("Invalid response body")
        end
    end

    local  data, err = socket.readline(fd, "\r\n")
    if not data then
        return false,err
    end

    return chunkdata
end

local function response_handler(fd)
    local data, err = socket.readline(fd, "\r\n\r\n")
    assert(data,err)

    --print("raw data",data)
    local ok, version, status_code, header = parse_response(data)
    assert(ok,"Invalid HTTP response header")

    local response = {
        version = version,
        status_code = status_code,
        header = header
    }

    response.header = {}
    for k,v in pairs(header) do
        response.header[k:lower()]=v
    end

    header = response.header

    local content_length = header["content-length"]
    if content_length then
        content_length = tonumber(content_length)
        if not content_length then
            error("content-length is not number")
        end

        if content_length >0 then
            --print("Content-Length",content_length)
            data, err = socket.read(fd, content_length)
            assert(data,err)
            response.content = data
        end
    elseif header["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd)
        if not chunkdata then
            error("Invalid response body")
        end
        response.content = tbconcat( chunkdata )
    else
        error ("Unsupport transfer-encoding:"..tostring(header["transfer-encoding"]))
    end
    return response
end

local function parse_host(host, defaultport)
    local host_, port = host:match("([^:]+):?(%d*)$")
    if port == "" then
        port = defaultport
    else
        port = tonumber(port)
    end
    return host_, port
end

local M = {}

local timeout  = 0

local proxyaddress = nil

local pool = {}

local max_connection_num = 10

local function do_request(baseaddress, keepalive, req)

    local fd
    local err
    local fdpool = pool[baseaddress]
    if not fdpool then
        fdpool = {}
        pool[baseaddress] = fdpool
    end

    if #fdpool == 0 then
        local host, port = parse_host(baseaddress, 80)
        fd, err = socket.connect(host, port,  moon.PTYPE_TEXT, timeout)
        if not fd then
            return false ,err
        end
        socket.settimeout(fd, timeout//1000)
    else
        fd = table.remove(fdpool)
    end

    socket.write(fd, seri.concat(req))

    local ok , response = pcall(response_handler, fd)

    if not ok and #fdpool>0 then
        --request socket error remove all pool fd
        pool[baseaddress] = {}
    end

    if not keepalive then
        socket.close(fd)
    else
        if ok and keepalive then
            if #fdpool < max_connection_num then
                table.insert(fdpool, fd)
            end
        end
    end
    return ok, response
end

local function request( method, baseaddress, path, content, header, keepalive)

    local host, port = parse_host(baseaddress, 80)

    if not path or path=="" then
        path = "/"
    end

    if proxyaddress then
        path = "http://"..host..':'..port..path
    end

    local cache = {}
    tbinsert( cache, method )
    tbinsert( cache, " " )
    tbinsert( cache, path )
    tbinsert( cache, " HTTP/1.1\r\n" )
    tbinsert( cache, "Host: " )
    tbinsert( cache, host )
    tbinsert( cache, "\r\n")

    if header then
        for k,v in pairs(header) do
            tbinsert( cache, k)
            tbinsert( cache, ": ")
            tbinsert( cache, tostring(v))
            tbinsert( cache, "\r\n")
        end
    end

    if content and #content > 0 then
        header = header or {}
        local v = header["Content-Length"]
        if not v then
            v = header["Transfer-Encoding"]
            if not v or v~="chunked" then
                tbinsert( cache, "Content-Length: ")
                tbinsert( cache, tostring(#content))
                tbinsert( cache, "\r\n")
            end
        end
    end

    if keepalive then
        tbinsert( cache, "Connection: keep-alive")
        tbinsert( cache, "\r\n")
        tbinsert( cache, "Keep-Alive: "..tostring(keepalive))
        tbinsert( cache, "\r\n")
    end
    tbinsert( cache, "\r\n")
    tbinsert( cache, content)


    if proxyaddress then
        baseaddress = proxyaddress
    end

    local ok, response = do_request(baseaddress, keepalive, cache)
    if not ok then
        --reconnect
        ok, response = do_request(baseaddress, keepalive, cache)
    end

    if ok then
        return response
    else
        return false, response
    end
end

function M.settimeout(v)
    timeout = v
end

function M.setproxy(host)
    proxyaddress = host
end

function M.get(host, path, content, header, keepalive)
    return request("GET", host, path, content, header, keepalive)
end

function M.put(host, path, content, header, keepalive)
    return request("PUT", host, path, content, header, keepalive)
end

function M.post(host, path, content, header, keepalive)
    return request("POST", host, path, content, header, keepalive)
end

function M.postform(host, path, form, header, keepalive)
    header = header or {}
    header["content-type"] = "application/x-www-form-urlencoded"

    for k,v in pairs(form) do
        form[k] = tostring(v)
    end
    return request("POST", host, path, create_query_string(form), header, keepalive)
end

return M

