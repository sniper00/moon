local moon = require("moon")
local socket = require("moon.socket")

local pcall = pcall
local error = error
local setmetatable = setmetatable

local read = socket.read

local socketchannel = {}

socketchannel.__index = socketchannel

function socketchannel.channel(opts)
    local obj = {}
    obj._opts = opts
    return setmetatable(obj, socketchannel)
end

function socketchannel:connect()
    local fd, err = socket.connect(self._opts.host, self._opts.port, moon.PTYPE_TEXT,self._opts.timeout)
    if not fd or fd ==0 then
        return {code = "SOCKET", err = err}
    end
    self._fd = fd
    if self._opts.auth then
        local ok, res = pcall(self._opts.auth, self)
        if not ok then
            return {code = "AUTH", err = res}
        end
    end
end

function socketchannel:close()
    socket.close(self._fd)
end

function socketchannel:response(resp)
    local _, data = resp(self)
    if not _ then
        error(data)
    end
    return data
end

function socketchannel:request(req,resp)
    socket.write(self._fd, req)
    local _, data = resp(self)
    if not _ then
        error(data)
    end
    return data
end

function socketchannel:read(len)
    return read(self._fd, len)
end

return socketchannel