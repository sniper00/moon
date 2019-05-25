local moon = require("moon")
local http = require("http")
local seri = require("seri")
local socket = require("moon.net.socket")


local tbinsert = table.insert
local tbconcat = table.concat

local strfmt = string.format

local tostring = tostring
local tonumber = tonumber
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

local http_response = {}

http_response.__index = http_response

function http_response.new()
    local o = {}
    o.header = {}
    return setmetatable(o, http_response)
end

function http_response:write_header(field,value)
    self.header[tostring(field)] = tostring(value)
end

function http_response:write(content)
    self.content = content
    self.header['Content-Length'] = #content
end

function http_response:tb()
    local status_code = self.status_code or 200
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
    self.header = {}
    self.status_code = nil
    self.content = nil
    return cache
end

----------------------------------------------------------

local M = {}

local routers = {}

local function read_chunked(session, chunkdata)
    local data, err = session:co_read('\r\n',M.chunk_max_len)--security
    if not data then
        return false,err
    end
    local length = tonumber(data)
    if not length then
        return false, "protocol error"
    end
    if length >0 then
        data, err = session:co_read(length)
        if not data then
            return false,err
        end
        tbinsert( chunkdata, data )
        data, err = session:co_read('\r\n',M.chunk_max_len)--security
        if not data then
            return false,err
        end
    elseif length <0 then
        return false, "protocol error"
    end
    return true
end

local function request_handler(session, request, data )
    local response = http_response.new()
    local conn_type = request:header("Connection")
    local handler =  routers[request.path]
    if handler then
        handler(request,response,data)
    end
    if conn_type == "close" then
        session:send(seri.concat(response:tb()),true)
    else
        session:send(seri.concat(response:tb()))
    end
end

local function session_handler(session,request)
    local data, err = session:co_read("\r\n\r\n",M.header_max_len)
    if not data then
        return false, err
    else
        --print("raw data",data)
        if request:parse(data) == -1 then
            return false, "header request error"
        end

        if request:has_header("Content-Length") then
            local content_length = tonumber(request:header("Content-Length"))
            if not content_length then
                return false, "Content-Length is not number"
            end

            if M.content_max_len and content_length> M.content_max_len then
                session:close()
                return false, strfmt( "content length %d, limit %d",content_length,M.content_max_len )
            end

            data, err = session:co_read(content_length)
            if not data then
                return false,err
            end
            --print("Content-Length",content_length)
            request_handler(session,request,data)
            return true
        elseif request:header("Transfer-Encoding") == 'chunked' then
            local chunkdata = {}
            local result,errmsg = read_chunked(session,chunkdata)
            if not result then
                return false,errmsg
            end
            request_handler(session,request,tbconcat( chunkdata ))
            return true
        end
        request_handler(session,request)
        return true
    end
end

-----------------------------------------------------------------

local server

function M.listen(ip,port,timeout)
    assert(not server,"http server can only listen port once.")
    server = socket.new()
    timeout = timeout or 0
    server:settimeout(timeout)
    server:listen(ip,port)
    moon.async(function()
        while true do
            local session,err = server:co_accept()
            if session then
                moon.async(function()
                    local request = http.request.new()
                    while true do
                        local ok, err2 = session_handler(session,request)
                        if not ok then
                            if M.error then
                                M.error(session,err2)
                            else
                                print("httpserver session error",err2)
                            end
                            return
                        end
                    end
                end)--async
            else
                print("httpserver accept",err)
            end--if
        end--while
    end)
end

function M.on( path, cb )
    routers[path] = cb
end

return M
