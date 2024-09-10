local moon = require("moon")
local socket = require("moon.socket")

local pcall = pcall
local error = error
local setmetatable = setmetatable

local read = socket.read

---@class socketchannel
local socketchannel = {}

socketchannel.__index = socketchannel

function socketchannel.channel(opts)
    local obj = {}
    obj._opts = opts
    obj._threads = {}
    return setmetatable(obj, socketchannel)
end

function socketchannel:connect(_)
    local fd, err = socket.connect(self._opts.host, self._opts.port, moon.PTYPE_SOCKET_TCP,self._opts.timeout)
    if not fd or fd ==0 then
        return {code = "SOCKET", message = err}
    end
    self._fd = fd

    local response = self._opts.response
    if response then
        moon.async(function ()
            while self._fd do
                local ok , session, result_ok, result_data = pcall(response, self)
                -- print(ok , session, result_ok, result_data)
                if ok and session then
                    local co = self._threads[session]
                    if co then
                        moon.wakeup(co, result_ok, result_data)
                    end
                else
                    socketchannel.close(self)
                    for k, co in pairs(self._threads) do
                        moon.wakeup(co, false, session)
                    end
                    self._threads = {}
                end
            end
        end)
    end

    if self._opts.auth then
        local ok, res = pcall(self._opts.auth, self)
        if not ok then
            return {code = "SOCKET", message = res}
        end
    end
end

function socketchannel:close()
    socket.close(self._fd)
    self._fd = false
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
    if resp == nil then
        -- no response
        return
    end
    if self._opts.response then
        self._threads[resp] = coroutine.running()
        local ok, data = moon.wait()
        if not ok then
            error(data)
        end
        return data
    end
    local _, data = resp(self)
    if not _ then
        return {code = "SOCKET", message = data}
    end
    return data
end

function socketchannel:read(len)
    return read(self._fd, len)
end

return socketchannel