--协程socket封装

local moon  = require("moon")
local close_flag = moon.buffer_flag.close
local co_yield = coroutine.yield
local make_response = moon.make_response
local type = type
local assert = assert
local tostring = tostring
local setmetatable = setmetatable
local tonumber = tonumber

local read_delim = {}
read_delim['\r\n'] = 1
read_delim['\r\n\r\n'] = 2
read_delim['\n'] = 3

local session       = {}

session.__index = session

function session.new(sock,connid,ip,host)
    local tb = {}
    tb.sock = sock
    tb.connid = connid
    tb.ip = ip
    tb.host = host
    return setmetatable(tb,session)
end

function session:co_read(n, maxbyte)
    local delim = 0
    if type(n) == 'string' then
        delim = read_delim[n]
        if not delim  then
            return nil,"unsupported read delim "..tostring(n)
        end
        n = maxbyte or 0
    end
    local respid = make_response()
    self.sock:read(self.connid,n,delim,respid)
    return co_yield()
end

function session:send(data,bclose)
    assert(self.connid,"attemp send an invalid session")
    if bclose then
        return self.sock:send_with_flag(self.connid,data,close_flag)
    else
        return self.sock:send(self.connid,data)
    end
end

function session:close()
    assert(self.connid,"attemp close an invalid session")
    local ret = self.sock:close(self.connid)
    self.connid = nil
    return ret
end

----------------------------------------------

local socket = {}

socket.__index = socket

local n = 0;
function socket.new()
    n=n+1
    local tb = {}
    tb.sock = moon.get_tcp("custom"..tostring(n))
    return setmetatable(tb,socket)
end

function socket:listen(ip,port)
    assert(self.sock:listen(ip,tostring(port)))
end

function socket:settimeout(t)
    self.sock:settimeout(t)
end

function socket:co_accept()
    local respid = make_response()
    self.sock:async_accept(respid)
    local connid,err = co_yield()
    if not connid then
        return nil,err
    end
    return session.new(self.sock,tonumber(connid))
end

function socket:connect(ip,port)
    local connid = self.sock:connect(ip,port)
    if connid == 0 then
        return nil,"connect failed"
    end
    return session.new(self.sock,tonumber(connid),ip,port)
end

function socket:co_connect(ip,port)
    local respid = make_response()
    self.sock:async_connect(ip,port,respid)
    local connid,err = co_yield()
    if not connid then
        return nil,err
    end
    return session.new(self.sock,tonumber(connid),ip,port)
end

return socket
