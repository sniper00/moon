local buffer = require("buffer")
local moon = require("moon")
local json = require("json")
local socket = require("moon.socket")
local c = require("http.core")

local string = string

local tbinsert = table.insert
local tbconcat = table.concat

local tostring = tostring
local tonumber = tonumber
local tointeger = math.tointeger
local pairs = pairs

local parse_response = c.parse_response

---@type fun(params:table):string
local create_query_string = c.create_query_string

-----------------------------------------------------------------

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
            tbinsert(chunkdata, data)
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

local function response_handler(fd, method)
    local data, err = socket.read(fd, "\r\n\r\n")
    if not data then
        return { error = err }
    end

    --print("raw data",data)
    ---@type HttpResponse
    local response, err = parse_response(data)
    if err then
        return { error = "Invalid HTTP response header" }
    end

    ---@diagnostic disable-next-line: assign-type-mismatch
    response.status_code = tointeger(string.match(response.status_code, "%d+"))

    local header = response.header

    if method == "HEAD" then
        return response
    end

    if header["transfer-encoding"] ~= 'chunked' and not header["content-length"] then
        header["content-length"] = "0"
    end

    local content_length = header["content-length"]
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
            response.content = data
        end
    elseif header["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd)
        if chunkdata.error then
            return chunkdata
        end
        response.content = tbconcat(chunkdata)
    else
        moon.warn("Unsupport transfer-encoding:" .. tostring(header["transfer-encoding"]))
    end

    return response
end

local M = {}

local default_connect_timeout <const> = 1000

local default_read_timeout <const> = 10000

local max_pool_num <const> = 10

local keep_alive_host = {}

---@param options HttpOptions
local function do_request(baseaddress, options, req, method)
    options.connect_timeout = options.connect_timeout or default_connect_timeout
    options.read_timeout = options.read_timeout or default_read_timeout

    local fd, err
    local pool = keep_alive_host[baseaddress]
    if not pool then
        pool = {}
        keep_alive_host[baseaddress] = pool
    end

    fd = table.remove(pool)

    if not fd then
        local host, port = socket.parse_host_port(baseaddress, 80)
        fd, err = socket.connect(host, port, moon.PTYPE_SOCKET_TCP, options.connect_timeout)
        if not fd then
            return { error = err }
        end
    end

    if not socket.write(fd, buffer.concat(req)) then
        return { error = "CLOSED" }
    end

    local read_timeout = options.read_timeout or 0
    socket.settimeout(fd, read_timeout // 1000)
    local ok, response = pcall(response_handler, fd, method)
    socket.settimeout(fd, 0)

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

    if not options.keepalive or response.header["connection"] == "close" or #pool >= max_pool_num then
        socket.close(fd)
    else
        table.insert(pool, fd)
    end

    return response
end

local function tojson(response)
    if response.status_code ~= 200 then return end
    return json.decode(response.content)
end

---@param method string
---@param baseaddress string
---@param options HttpOptions
---@param content? string
---@return HttpResponse
local function request(method, baseaddress, options, content)
    local host, port = socket.parse_host_port(baseaddress, 80)

    if not options.path or options.path == "" then
        options.path = "/"
    end

    if options.proxy then
        options.path = "http://" .. host .. ':' .. port .. options.path
    end

    local cache = {}
    cache[#cache + 1] = method
    cache[#cache + 1] = " "
    cache[#cache + 1] = options.path
    cache[#cache + 1] = " HTTP/1.1\r\n"
    cache[#cache + 1] = "Host: "
    cache[#cache + 1] = baseaddress
    cache[#cache + 1] = "\r\n"

    if options.header then
        for k, v in pairs(options.header) do
            cache[#cache + 1] = k
            cache[#cache + 1] = ": "
            cache[#cache + 1] = tostring(v)
            cache[#cache + 1] = "\r\n"
        end
    end

    if content and #content > 0 then
        options.header = options.header or {}
        local v = options.header["Content-Length"]
        if not v then
            v = options.header["Transfer-Encoding"]
            if not v or v ~= "chunked" then
                cache[#cache + 1] = "Content-Length: "
                cache[#cache + 1] = tostring(#content)
                cache[#cache + 1] = "\r\n"
            end
        end
    end

    if options.keepalive then
        cache[#cache + 1] = "Connection: keep-alive"
        cache[#cache + 1] = "\r\n"
        cache[#cache + 1] = "Keep-Alive: "
        cache[#cache + 1] = tostring(options.keepalive)
        cache[#cache + 1] = "\r\n"
    end
    cache[#cache + 1] = "\r\n"
    cache[#cache + 1] = content

    if options.proxy then
        baseaddress = options.proxy
    end

    local response = do_request(baseaddress, options, cache, method)
    if response.error then
        response = do_request(baseaddress, options, cache, method)
    end

    if not response.error then
        response.json = tojson
        return response
    else
        return { status_code = -1, content = response.error }
    end
end

M.create_query_string = create_query_string

---@class HttpOptions
---@field public path string
---@field public header table<string,string>
---@field public keepalive integer @ seconds
---@field public connect_timeout integer @ ms
---@field public read_timeout integer @ ms
---@field public proxy string @ host:port

---@class HttpResponse
---@field public version string @ http version
---@field public status_code integer @ Integer Code of responded HTTP Status, e.g. 404 or 200. -1 means socket error and content is error message
---@field public header table<string,string> @in lower-case key
---@field public content string @ raw body string
---@field public json fun(response:HttpResponse):table @ Returns the json-encoded content of a response, if decode failed return nil and error string. if status_code not 200, return nil


---@param host string @host:port
---@param options? HttpOptions
---@return HttpResponse
function M.get(host, options)
    options = options or {}
    return request("GET", host, options)
end

---@param host string @host:port
---@param options HttpOptions
---@return HttpResponse
function M.head(host, options)
    options = options or {}
    return request("HEAD", host, options)
end

---@param host string @host:port
---@param options HttpOptions
---@return HttpResponse
function M.put(host, content, options)
    options = options or {}
    return request("PUT", host, options, content)
end

---@param host string @host:port
---@param content string
---@param options HttpOptions
---@return HttpResponse
function M.post(host, content, options)
    options = options or {}
    return request("POST", host, options, content)
end

---@param host string @host:port
---@param form table @
---@param options HttpOptions
---@return HttpResponse
function M.postform(host, form, options)
    options = options or {}
    options.header = options.header or {}
    options.header["content-type"] = "application/x-www-form-urlencoded"
    for k, v in pairs(form) do
        form[k] = tostring(v)
    end
    return request("POST", host, options, create_query_string(form))
end

return M
