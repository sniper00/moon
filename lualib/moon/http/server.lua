local moon = require("moon")
local http = require("http")
local seri = require("seri")
---@type fs
local fs = require("fs")
local socket = require("moon.socket")

local parse_request = http.parse_request
local parse_query_string = http.parse_query_string

local tbinsert = table.insert
local tbconcat = table.concat

local strfmt = string.format

local tostring = tostring
local tonumber = tonumber
local tointeger = math.tointeger
local setmetatable = setmetatable
local pairs = pairs
local assert = assert

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

local mimes = {
    [".css"] = "text/css",
    [".htm"] = "text/html; charset=UTF-8",
    [".html"] = "text/html; charset=UTF-8",
    [".js"] = "application/x-javascript",
    [".jpeg"] = "image/jpeg",
    [".ico"] = "image/x-icon"
}

---@class HttpServerResponse
---@field header table<string, any>
---@field status_code integer
---@field content? string
local http_response = {}

http_response.__index = http_response

local static_content

function http_response.new()
    local o = {}
    o.header = {}
    o.status_code = 200
    return setmetatable(o, http_response)
end

---@param field string
---@param value string
function http_response:write_header(field,value)
    self.header[tostring(field)] = tostring(value)
end

---@param content string
function http_response:write(content)
    self.content = content
    if content then
        self.header['Content-Length'] = #content
    end
end

function http_response:tb()
    local status_code = self.status_code
    local status_msg = http_status_msg[status_code]
    assert(status_msg,"invalid http status code")

    local cache = {}
    tbinsert( cache, "HTTP/1.1 "..tostring(status_code).." " )
    tbinsert( cache, status_msg )
    tbinsert( cache, "\r\n" )

    for k,v in pairs(self.header) do
        tbinsert( cache, k )
        tbinsert( cache, ": " )
        tbinsert( cache, v )
        tbinsert( cache, "\r\n" )
    end
    tbinsert( cache, "\r\n" )
    if self.content then
        tbinsert( cache, self.content )
    end
    return cache
end

----------------------------------------------------------

local M = {}
--header and content max length
M.content_max_len = false
--is enable keepalvie
M.keepalive = true

local routers = {}

local function read_chunked(fd, content_max_len)

    local chunkdata = {}
    local content_length = 0

    while true do
        local data, err = socket.read(fd, "\r\n", 64)
        if not data then
            return {socket_error = err}
        end

        local length = tonumber(data, 16)
        if not length then
            return {protocol_error = "Invalid chunked format:"..data}
        end

        if length==0 then
            break
        end

        content_length = content_length + length

        if content_max_len and content_length > content_max_len then
            return {protocol_error = strfmt( "HTTP content length exceeded %d, request length %d", content_max_len, content_length)}
        end

        if length >0 then
            data, err = socket.read(fd, length)
            if not data then
                return {socket_error = err}
            end
            tbinsert( chunkdata, data )
            data, err = socket.read(fd, "\r\n", 2)
            if not data then
                return {socket_error = err}
            end
        elseif length <0 then
            return {protocol_error = "Invalid chunked format:"..length}
        end
    end

    local  data, err = socket.read(fd, "\r\n", 2)
    if not data then
        return {socket_error = err}
    end

    return chunkdata
end

local traceback = debug.traceback

---return keepalive
local function request_handler(fd, request)
    local response = http_response.new()

    if static_content and (request.method == "GET" or request.method == "HEAD") then
        local request_path = request.path
        local static_src = static_content[request_path]
        if static_src then
            response:write_header("Content-Type", static_src.mime)
            response:write(static_src.bin)
            if not M.keepalive or request.header["connection"] == "close" then
                response:write_header("Connection", "close")
                socket.write_then_close(fd, seri.concat(response:tb()))
                return
            else
                socket.write(fd, seri.concat(response:tb()))
                return true
            end
        end
    end

    local handler =  routers[request.path]
    if handler then
        local ok,err = xpcall(handler, traceback, request, response)
        if not ok then
            if M.error then
                M.error(fd, err)
            else
                moon.error(err)
            end

            request.header["connection"] = "close"

            response.status_code = 500
            response:write_header("Content-Type","text/plain")
            response:write("Server Internal Error")
        end
    else
        response.status_code = 404
        response:write_header("Content-Type","text/plain")
        response:write(string.format("Cannot %s %s", request.method, request.path))
    end

    if not M.keepalive or request.header["connection"] == "close" then
        response:write_header("Connection", "close")
        socket.write_then_close(fd, seri.concat(response:tb()))
    else
        socket.write(fd, seri.concat(response:tb()))
        return true
    end
end

local function read_request(fd, pre)
    local data, err = socket.read(fd, "\r\n\r\n", M.header_max_len)
    if not data then
        return {socket_error = err}
    end

    if pre then
        data = pre..data
    end

    --print("raw data",data)
    local ok,method,path,query_string,version,header = parse_request(data)
    if not ok then
        return {protocol_error = "Invalid HTTP request header"}
    end

    ---@class HttpServerRequest
    ---@field method string
    ---@field path string
    ---@field query_string string
    ---@field version string
    ---@field content string
    local request = {
        method = method,
        path = path,
        query_string = query_string,
        version = version
    }

    request.header = {}
    for k,v in pairs(header) do
        request.header[k:lower()]=v
    end

    header = request.header

    if method == "HEAD" then
        return request
    end

    request.parse_query = function() return parse_query_string(request.query_string) end

    request.parse_form = function() return parse_query_string(request.content) end

    if header["transfer-encoding"] ~= 'chunked' and not header["content-length"] then
        header["content-length"] = "0"
    end

    local content_length = header["content-length"]
    if content_length then
        content_length = tointeger(content_length)
        if not content_length then
            return {protocol_error = "Content-length is not number"}
        end

        if M.content_max_len and content_length> M.content_max_len then
            return {protocol_error = strfmt( "HTTP content length exceeded %d, request length %d", M.content_max_len, content_length)}
        end

        data, err = socket.read(fd, content_length)
        if not data then
            return {socket_error = err}
        end
        --print("Content-Length",content_length)
        request.content = data
    elseif header["transfer-encoding"] == 'chunked' then
        local chunkdata = read_chunked(fd, M.content_max_len)
        if chunkdata.socket_error or chunkdata.protocol_error then
            return chunkdata
        end
        request.content = tbconcat( chunkdata )
    else
        return {protocol_error = "Unsupport transfer-encoding:"..header["transfer-encoding"]}
    end

    return request
end

-----------------------------------------------------------------

local listenfd

---@param timeout integer @read timeout in seconds
---@param pre? string @first request prefix data, for convert a tcp request to http request
function M.start(fd, timeout, pre)
    socket.settimeout(fd, timeout)
    moon.async(function()
        while true do
            local request = read_request(fd, pre)
            if pre then
                pre = nil
            end
            if request.socket_error then
                socket.close(fd)
                return
            end

            if request.protocol_error then
                local res = http_response.new()
                res.status_code = 400
                socket.write_then_close(fd, seri.concat(res:tb()))
                if M.error then
                    M.error(fd, request.protocol_error)
                else
                    moon.error("HTTP_SERVER_ERROR: "..request.protocol_error)
                end
                return
            end

            if not request_handler(fd, request) then
                return
            end
        end
    end)--async
end

---@param host string @ ip address
---@param port integer @ port
---@param timeout? integer @read timeout in seconds
function M.listen(host,port,timeout)
    assert(not listenfd,"http server can only listen port once.")
    listenfd = socket.listen(host, port, moon.PTYPE_SOCKET_TCP)
    timeout = timeout or 0
    moon.async(function()
        while true do
            local fd,err = socket.accept(listenfd, moon.id)
            if not fd then
                print("httpserver accept",err)
                return
            end
            M.start(fd, timeout)
        end--while
    end)
end

---@param cb fun(request: HttpServerRequest, response: HttpServerResponse)
function M.on( path, cb )
    routers[path] = cb
end

function M.static(dir, showdebug)
    static_content = {}
    dir = fs.abspath(dir)
    local res = fs.listdir(dir, 100)
    for _, v in ipairs(res) do
        local pos = string.find(v, dir, 1, true)
        local src = string.sub(v, pos+#dir)
        src = string.gsub(src, "\\","/")
        if string.sub(src,1,1) ~= "/" then
            src = "/"..src
        end

        if not fs.isdir(v) then
            local ext = fs.ext(src)
            local mime = mimes[ext] or ext
            static_content[src] = {
                mime = mime,
                bin = io.readfile(v)
            }
            if showdebug then
                print("load static file:", src)
            end
        else
            local index_html = fs.join(v,"index.html")
            if fs.exists(index_html) then
                static_content[src] = {
                    mime = mimes[".html"],
                    bin = io.readfile(index_html)
                }
                static_content[src.."/"] = static_content[src]
                if showdebug then
                    print("load static index:", src)
                    print("load static index:", src.."/")
                end
            end
        end
    end

    if fs.exists(fs.join(dir,"index.html")) then
        static_content["/"] = {
            mime = mimes[".html"],
            bin = io.readfile(fs.join(dir,"index.html"))
        }
    end
end

return M
