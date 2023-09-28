local c = require("http.core")
local internal = require("moon.http.internal")

local tostring = tostring
local pairs = pairs

---@type fun(params:table):string
local create_query_string = c.create_query_string

-----------------------------------------------------------------

local M = {}

M.create_query_string = create_query_string

---@param host string @host:port
---@param options? HttpOptions
---@return HttpResponse
function M.get(host, options)
    options = options or {}
    return internal.request("GET", host, options)
end

---@param host string @host:port
---@param options HttpOptions
---@return HttpResponse
function M.head(host, options)
    options = options or {}
    return internal.request("HEAD", host, options)
end

---@param host string @host:port
---@param options HttpOptions
---@return HttpResponse
function M.put(host, content, options)
    options = options or {}
    return internal.request("PUT", host, options, content)
end

---@param host string @host:port
---@param content string
---@param options HttpOptions
---@return HttpResponse
function M.post(host, content, options)
    options = options or {}
    return internal.request("POST", host, options, content)
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
    return internal.request("POST", host, options, create_query_string(form))
end

return M
