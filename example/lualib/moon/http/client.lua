local http = require("http")
local seri = require("seri")
local moon = require("moon")
local socket = require("moon.socket")

local tbinsert = table.insert
local tbconcat = table.concat

local tostring = tostring
local tonumber = tonumber
local setmetatable = setmetatable
local pairs = pairs

local parse_response = http.parse_response

-----------------------------------------------------------------

local function parse_host_port( host_port, default_port )
    local start,e = host_port:find(':')
    if start and e then
        return {host_port:sub(1,start-1),host_port:sub(e+1,-1)}
    else
        return {host_port,tostring(default_port)}
    end
end

local function read_chunked(fd, chunkdata)
    local data, err = socket.readline(fd, "\r\n")
    if not data then
        return false,err
    end
    local length = tonumber(data,"16")
    if not length then
        return false, "protocol error"
    end
    if length >0 then
        data, err = socket.read(fd, length)
        if not data then
            return false,err
        end
        tbinsert( chunkdata, data )
        data, err = socket.readline(fd, "\r\n")
        if not data then
            return false,err
        end
    elseif length <0 then
        return false, "protocol error"
    else
        return "E"
    end
    return true
end

local function response_handler(fd)
    local data, err = socket.readline(fd, "\r\n\r\n")
    if not data then
        return false, err
    else
        --print("raw data",data)
        local ok, version, status_code, header = parse_response(data)
        if not ok then
            return false, "header response error"
        end
        local response = {
            version = version,
            status_code = status_code,
            header = header
        }

        local content_length = header["Content-Length"]
        if content_length then
            content_length = tonumber(content_length)
            if not content_length then
                return false, "Content-Length is not number"
            end
            --print("Content-Length",content_length)
            data, err = socket.read(fd, content_length)
            if not data then
                return false,err
            end
            response.content = data
        elseif header["Transfer-Encoding"] == 'chunked' then
            local chunkdata = {}
            while true do
            local result,errmsg = read_chunked(fd,chunkdata)
            if not result then
                return false,errmsg
            end
                if result == "E" then
                    break
                end
            end
            data, err = socket.readline(fd, "\r\n")
            if not data then
                return false,err
            end
            response.content = tbconcat( chunkdata )
        end
        return response
    end
end

local default_port = 80

local M = {}

M.__index = M

function M.new(host_port, timeout, proxy)
    local o = {}
    o.host = parse_host_port(host_port,default_port)
    if proxy then
        o.proxy =  parse_host_port(proxy,8080)
    end
    o.timeout = timeout
    return setmetatable(o, M)
end

function M:close()
    if self.fd then
        socket.close(self.fd)
        self.fd = nil
    end
end

function M:request( method, path, content, header)
    if not path or path=="" then
        path = "/"
    end
    if self.proxy then
        path = "http://"..self.host[1]..':'..self.host[2]..path
    end

    local cache = {}
    tbinsert( cache, method )
    tbinsert( cache, " " )
    tbinsert( cache, path )
    tbinsert( cache, " HTTP/1.1\r\n" )
    tbinsert( cache, "Host: " )
    tbinsert( cache, self.host[1] )
    if self.host[2] ~= tostring(default_port) then
        tbinsert( cache, ":" )
        tbinsert( cache, self.host[2] )
    end
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
    tbinsert( cache, "\r\n")
    tbinsert( cache, content)

    if not self.fd then
        local fd,err
        if self.proxy then
            fd,err = socket.connect(self.proxy[1], tonumber(self.proxy[2]),  moon.PTYPE_TEXT)
        else
            --print(self.host[1],self.host[2])
            fd,err = socket.connect(self.host[1], tonumber(self.host[2]),  moon.PTYPE_TEXT)
        end
        if not fd then
            return false,err
        end
        self.fd = fd
    end

    --print(seri.concats(cache))
    socket.write(self.fd, seri.concat(cache))

    local response,err = response_handler(self.fd,self.response)
    if not response then
        return false, err
    end
    return response
end

return M

