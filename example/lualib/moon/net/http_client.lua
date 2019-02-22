local moon = require("moon")
local seri = require("seri")
local socket = require("moon.net.socket")

-----------------------------------------------------------------

local function parse_host_port( host_port, default_port )
    local start,e = host_port:find(':')
    if start and e then
        return {host_port:sub(1,start-1),host_port:sub(e+1,-1)}
    else
        return {host_port,tostring(default_port)}
    end
end

local function read_chunked(session, chunkdata)
    local data, err = session:co_read('\r\n')
    if not data then
        return false,err
    end
    local length = tonumber(data,"16")
    if not length then
        return false, "protocol error"
    end
    if length >0 then
        data, err = session:co_read(length)
        if not data then
            return false,err
        end
        table.insert( chunkdata, data )
        data, err = session:co_read('\r\n')
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

local function response_handler(conn,parser)
    local data, err = conn:co_read("\r\n\r\n")
    if not data then
        return false, err
    else
        --print("raw data",data)
        if parser:parse(data) == -1 then
            return false, "header parser error"
        end

        if parser:has_header("Content-Length") then
            local content_length = tonumber(parser:header("Content-Length"))
            if not content_length then
                return false, "Content-Length is not number"
            end
            --print("Content-Length",content_length)
            data, err = conn:co_read(content_length)
            if not data then
                return false,err
            end
            return data
        elseif parser:header("Transfer-Encoding") == 'chunked' then
            local chunkdata = {}
            while true do
            local result,errmsg = read_chunked(conn,chunkdata)
            if not result then
                return false,errmsg
            end
                if result == "E" then
                    break
                end
            end
            data, err = conn:co_read('\r\n')
            if not data then
                return false,err
            end
            return table.concat( chunkdata )
        end
        return ""
    end
end

local default_port = 80

local sock = socket.new()

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
    if self.conn then
        self.conn:close()
        self.conn = nil
    end
end

function M:request( method,path,content,header)
    if not path or path=="" then
        path = "/"
    end
    if self.proxy then
        path = "http://"..self.host[1]..':'..self.host[2]..path
    end

    local cache = {}
    table.insert( cache, method )
    table.insert( cache, " " )
    table.insert( cache, path )
    table.insert( cache, " HTTP/1.1\r\n" )
    table.insert( cache, "Host: " )
    table.insert( cache, self.host[1] )
    if self.host[2] ~= tostring(default_port) then
        table.insert( cache, ":" )
        table.insert( cache, self.host[2] )
    end
    table.insert( cache, "\r\n")

    if header then
        for k,v in pairs(header) do
            table.insert( cache, k)
            table.insert( cache, ": ")
            table.insert( cache, tostring(v))
            table.insert( cache, "\r\n")
        end
    end

    if content and #content > 0 then
        header = header or {}
        local v = header["Content-Length"]
        if not v then
            v = header["Transfer-Encoding"]
            if not v or v~="chunked" then
                table.insert( cache, "Content-Length: ")
                table.insert( cache, tostring(#content))
                table.insert( cache, "\r\n")
            end
        end
    end
    table.insert( cache, "\r\n")
    table.insert( cache, content)

    if not self.conn then
        local conn,err
        if self.proxy then
            conn,err = sock:co_connect(self.proxy[1],self.proxy[2])
        else
            --print(self.host[1],self.host[2])
            conn,err = sock:co_connect(self.host[1],self.host[2])
        end
        if not conn then
            return false,err
        end
        self.conn = conn
        self.parser = moon.http_response_parser.new()
    end

    --print(seri.concats(cache))
    self.conn:send(seri.concat(cache))

    local data,err = response_handler(self.conn,self.parser)
    if not data then
        return false, err
    end
    return self.parser,data
end

return M

