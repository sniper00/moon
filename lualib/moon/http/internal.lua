---@class HttpOptions
---@field public headers? table<string,string>
---@field public timeout? integer @ The timeout is applied from when the request starts connecting until the response body has finished. milliseconds, default 10000
---@field public proxy? string @ host:port

---@class HttpRequest
---@field address string
---@field method string
---@field path string
---@field headers table<string,string>
---@field query_string string
---@field version string
---@field body string
---@field parse_query fun(request:HttpRequest):table<string,string>
---@field parse_form fun(request:HttpRequest):table<string,string>

local moon = require("moon")
local json = require("json")
local buffer = require("buffer")
local socket = require("moon.socket")
local c = require("http.core")


local table = table
local string = string
local tostring = tostring
local setmetatable = setmetatable
local pairs = pairs
local tointeger = math.tointeger

---@type fun(params:string):string
local urlencode = c.urlencode
---@type fun(params:string):string
local urldecode = c.urldecode

local default_timeout <const> = 10000 -- 10s
local max_pool_num <const> = 10
local keep_alive_host = {}

local http_status_msg = {

    [100] = "Continue",

    [101] = "Switching Protocols",

    [200] = "OK",

    [201] = "Created",

    [202] = "Accepted",

    [203] = "Non-Authoritative Information",

    [204] = "No Content",

    [205] = "Reset Content",

    [206] = "Partial Content",

    [300] = "Multiple Choices",

    [301] = "Moved Permanently",

    [302] = "Found",

    [303] = "See Other",

    [304] = "Not Modified",

    [305] = "Use Proxy",

    [307] = "Temporary Redirect",

    [400] = "Bad Request",

    [401] = "Unauthorized",

    [402] = "Payment Required",

    [403] = "Forbidden",

    [404] = "Not Found",

    [405] = "Method Not Allowed",

    [406] = "Not Acceptable",

    [407] = "Proxy Authentication Required",

    [408] = "Request Time-out",

    [409] = "Conflict",

    [410] = "Gone",

    [411] = "Length Required",

    [412] = "Precondition Failed",

    [413] = "Request Entity Too Large",

    [414] = "Request-URI Too Large",

    [415] = "Unsupported Media Type",

    [416] = "Requested range not satisfiable",

    [417] = "Expectation Failed",

    [500] = "Internal Server Error",

    [501] = "Not Implemented",

    [502] = "Bad Gateway",

    [503] = "Service Unavailable",

    [504] = "Gateway Time-out",

    [505] = "HTTP Version not supported",
}

---@class HttpResponse
---@field public version string @ http version
---@field public status_code integer @ Integer Code of responded HTTP Status, e.g. 404 or 200. -1 means socket error and content is error message
---@field public headers table<string,any> @in lower-case key
---@field public body string @ raw body string
---@field public json? fun(response:HttpResponse):table @ Returns the json-encoded content of a response, if decode failed return nil and error string. if status_code not 200, return nil
---@field public socket_fd? integer @ socket fd if response.headers["connection"]:lower() == "upgrade"
local http_response = {}

http_response.__index = http_response

function http_response.new()
    local o = {}
    o.headers = {}
    o.status_code = 200
    return setmetatable(o, http_response)
end

---@param field string
---@param value string
function http_response:write_header(field, value)
    self.headers[tostring(field)] = tostring(value)
end

---@param body string
function http_response:write(body)
    self.body = body
    if body then
        self.headers['Content-Length'] = #body
    end
end

function http_response:tb()
    local status_code = self.status_code
    local status_msg = http_status_msg[status_code]
    if not status_msg then
        error("invalid http status code")
    end

    local cache = {}
    cache[#cache + 1] = "HTTP/1.1 "
    cache[#cache + 1] = tostring(status_code)
    cache[#cache + 1] = " "
    cache[#cache + 1] = status_msg
    cache[#cache + 1] = "\r\n"

    for k, v in pairs(self.headers) do
        cache[#cache + 1] = k
        cache[#cache + 1] = ": "
        cache[#cache + 1] = v
        cache[#cache + 1] = "\r\n"
    end
    cache[#cache + 1] = "\r\n"
    if self.body then
        cache[#cache + 1] = self.body
    end
    return cache
end

local function read_chunked(fd, content_max_len)
    local chunkdata = {}
    local content_length = 0

    while true do
        local data, err = socket.read(fd, "\r\n", 64)
        if not data then
            return { error = err }
        end

        local length = tonumber(data, 16)
        if not length then
            return { error = "Invalid chunked format:" .. data }
        end

        if length == 0 then
            break
        end

        content_length = content_length + length

        if content_max_len and content_length > content_max_len then
            return { error = string.format("Content length %d, limit %d", content_length, content_max_len) }
        end

        if length > 0 then
            data, err = socket.read(fd, length)
            if not data then
                return { error = err }
            end
            table.insert(chunkdata, data)
            data, err = socket.read(fd, "\r\n", 2)
            if not data then
                return { error = err }
            end
        elseif length < 0 then
            return { error = "Invalid chunked format:" .. length }
        end
    end

    local data, err = socket.read(fd, "\r\n", 2)
    if not data then
        return { error = err }
    end

    return chunkdata
end

local function read_response(fd, method)
    local data, err = socket.read(fd, "\r\n\r\n")
    if not data then
        return { error = err }
    end

    --print("raw data",data)
    ---@type HttpResponse
    local response, err = c.parse_response(data)
    if err then
        return { error = "Invalid HTTP response headers" }
    end

    ---@diagnostic disable-next-line: assign-type-mismatch
    response.status_code = tointeger(string.match(response.status_code, "%d+"))

    local headers = response.headers

    if method == "HEAD" then
        return response
    end

    if headers["transfer-encoding"] ~= 'chunked' and not headers["content-length"] then
        headers["content-length"] = "0"
    end

    local content_length = headers["content-length"]
    if content_length then
        ---@diagnostic disable-next-line: cast-local-type
        content_length = tointeger(content_length)
        if not content_length then
            return { error = "Content-length is not an integer" }
        end

        if content_length > 0 then
            --print("Content-Length",content_length)
            data, err = socket.read(fd, content_length)
            if not data then
                return { error = err }
            end
            response.body = data
        end
    elseif headers["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd)
        if chunkdata.error then
            return chunkdata
        end
        response.body = table.concat(chunkdata)
    else
        return { error = "Unsupport transfer-encoding:" .. tostring(headers["transfer-encoding"]) }
    end

    return response
end

local function parse_query(request)
    return c.parse_query_string(request.query_string)
end

local function parse_form(request)
    return c.parse_query_string(request.body)
end

local function parse_header(data)
    --print("raw data",data)
    ---@type HttpRequest
    local request, err = c.parse_request(data)
    if err then
        return { protocol_error = "Invalid HTTP request headers" }
    end

    if request.method == "HEAD" then
        return request
    end

    request.parse_query = parse_query

    request.parse_form = parse_form

    return request
end

local M = {
    http_response = http_response,
    parse_header = parse_header,
    urlencode = urlencode,
    urldecode = urldecode,
}

function M.read_request(fd, prefix_data, opt)
    local header_max_len = opt.header_max_len or 8192
    local content_max_len = opt.content_max_len or 5242880 -- 5MB

    local data, err = socket.read(fd, "\r\n\r\n", header_max_len)
    if not data then
        return { error = err, network_error = true }
    end

    if prefix_data then
        data = prefix_data .. data
    end

    local request = parse_header(data)
    if request.protocol_error then
        return request
    end

    local headers = request.headers

    if headers["transfer-encoding"] ~= 'chunked' and not headers["content-length"] then
        headers["content-length"] = "0"
    end

    local content_length = headers["content-length"]
    if content_length then
        ---@diagnostic disable-next-line: cast-local-type
        content_length = tointeger(content_length)
        if not content_length then
            return { error = "Content-length is not number" }
        end

        if content_length == 0 then
            request.body = ""
            return request
        end

        if content_max_len and content_length > content_max_len then
            return {
                error = string.format("HTTP content length exceeded %d, request length %d", content_max_len,
                    content_length)
            }
        end

        data, err = socket.read(fd, content_length)
        if not data then
            return { error = err, network_error = true }
        end
        --print("Content-Length",content_length)
        request.body = data
    elseif headers["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd, content_max_len)
        if chunkdata.error then
            return chunkdata
        end
        request.body = table.concat(chunkdata)
    else
        return { error = "Unsupport transfer-encoding:" .. headers["transfer-encoding"] }
    end

    return request
end

---@param options HttpOptions
local function do_request(baseaddress, options, req, method, protocol)
    options.timeout = options.timeout or default_timeout

    local fd, err
    local pool = keep_alive_host[baseaddress]
    if not pool then
        pool = {}
        keep_alive_host[baseaddress] = pool
    end

    fd = table.remove(pool)

    local timeout = options.timeout

    if not fd then
        local host, port = baseaddress:match("([^:]+):?(%d*)$")
        port = math.tointeger(port) or (protocol == 'https' and 443 or 80)
        local start_time = moon.clock()
        fd, err = socket.connect(host, port, moon.PTYPE_SOCKET_TCP, timeout)
        if not fd then
            return { error = err }
        end
        timeout = timeout - math.floor((moon.clock() - start_time) * 1000)
    end

    if not socket.write(fd, buffer.concat(req)) then
        return { error = "CLOSED" }
    end

    socket.settimeout(fd, math.max(timeout // 1000, 1))
    local ok, response = pcall(read_response, fd, method)

    if not ok then
        socket.close(fd)
        return { error = response }
    end

    if response.error then
        socket.close(fd)

        repeat
            local v = table.remove(pool)
            if v then
                socket.close(v)
            end
        until not v

        return response
    end

    if tostring(response.headers["connection"]):lower() == "upgrade" then
        response.socket_fd = fd
    elseif response.headers["connection"] == "close" or #pool >= max_pool_num then
        socket.close(fd)
    else
        table.insert(pool, fd)
    end
    return response
end

local function tojson(response)
    if response.status_code ~= 200 then return end
    return json.decode(response.body)
end

local function check_protocol(host)
    local protocol = host:match("^[Hh][Tt][Tt][Pp][Ss]?://")
    if protocol then
        host = string.gsub(host, "^" .. protocol, "")
        protocol = string.lower(protocol)
        if protocol == "https://" then
            return "https", host
        elseif protocol == "http://" then
            return "http", host
        else
            error(string.format("Invalid protocol: %s", protocol))
        end
    else
        return "http", host
    end
end

function M.parse_url(url)
    local protocol_end = url:find("://")
    local protocol = ""
    local host = ""
    local port = ""
    local path = ""
    local query_string = ""
    local host_start, host_end, path_start, query_start

    if protocol_end then
        protocol = url:sub(1, protocol_end - 1)
        host_start = protocol_end + 3
    else
        host_start = 1
    end

    host_end = url:find("[:/%?]", host_start) or #url + 1
    host = url:sub(host_start, host_end - 1)

    if url:sub(host_end, host_end) == ":" then
        local port_start = host_end + 1
        path_start = url:find("[/%?]", port_start) or #url + 1
        port = url:sub(port_start, path_start - 1)
    else
        path_start = host_end
    end

    query_start = url:find("?", path_start) or #url + 1
    path = url:sub(path_start, query_start - 1)
    local fragment_start = url:find("#", query_start + 1) or #url + 1
    query_string = url:sub(query_start + 1, fragment_start - 1)

    local query = {}
    for key, value in query_string:gmatch("([^&=?]-)=([^&=?]*)") do
        query[key] = urldecode(value)
    end

    return {
        protocol = protocol,
        host = host,
        port = port,
        path = path,
        query = query,
        query_string = query_string
    }
end

---@param method string
---@param str_url string
---@param options HttpOptions
---@param body? string
---@return HttpResponse
function M.request(method, str_url, options, body)
    local url = M.parse_url(str_url)
    local protocol = url.protocol
    local host = url.host
    if url.port and url.port ~= "" then
        host = host .. ":" .. url.port
    end
    if protocol ~= "https" and protocol ~= "http" then
        error(string.format("Invalid protocol: %s", protocol))
    end

    if not url.path or url.path == "" then
        url.path = "/"
    end

    if url.query and next(url.query) then
        url.query_string = c.create_query_string(url.query)
        url.path = url.path .. "?" .. url.query_string
    end

    if protocol == "https" then
        options.proxy = options.proxy or moon.env("HTTPS_PROXY")
    end

    if options.proxy then
        url.path = string.format("%s://%s%s", protocol, host, url.path)
    else
        if protocol == "https" then
            error(
                'Error: The protocol is set to "https", but no HTTP to HTTPS forward proxy has been set. Please set a proxy using the "options.proxy" parameter.')
        end
    end

    local cache = {}
    cache[#cache + 1] = method
    cache[#cache + 1] = " "
    cache[#cache + 1] = url.path
    cache[#cache + 1] = " HTTP/1.1\r\n"
    cache[#cache + 1] = "Host: "
    cache[#cache + 1] = host
    cache[#cache + 1] = "\r\n"

    if options.headers then
        for k, v in pairs(options.headers) do
            cache[#cache + 1] = k
            cache[#cache + 1] = ": "
            cache[#cache + 1] = tostring(v)
            cache[#cache + 1] = "\r\n"
        end
    end

    if body and #body > 0 then
        options.headers = options.headers or {}
        local v = options.headers["Content-Length"]
        if not v then
            v = options.headers["Transfer-Encoding"]
            if not v or v ~= "chunked" then
                cache[#cache + 1] = "Content-Length: "
                cache[#cache + 1] = tostring(#body)
                cache[#cache + 1] = "\r\n"
            end
        end
    end

    cache[#cache + 1] = "Connection: keep-alive\r\n"
    cache[#cache + 1] = "Keep-Alive: 300\r\n"

    cache[#cache + 1] = "\r\n"
    cache[#cache + 1] = body

    if options.proxy then
        protocol, host = check_protocol(options.proxy)
    end

    local response = do_request(host, options, cache, method, protocol)
    if response.error then
        response = do_request(host, options, cache, method, protocol)
    end

    if not response.error then
        response.json = tojson
        return response
    else
        return { status_code = -1, body = response.error }
    end
end

return M
