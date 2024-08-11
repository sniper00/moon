local c = require("http.core")
local json = require "json"
local internal = require("moon.http.internal")

local tostring = tostring
local pairs = pairs

---@type fun(params:table):string
local create_query_string = c.create_query_string

-----------------------------------------------------------------

local M = {}

M.create_query_string = create_query_string

---@param url string
---@param options? HttpOptions
---@return HttpResponse
function M.get(url, options)
    options = options or {}
    return internal.request("GET", url, options)
end

---@param url string
---@param options? HttpOptions
---@return HttpResponse
function M.head(url, options)
    options = options or {}
    return internal.request("HEAD", url, options)
end

---@param url string
---@param options? HttpOptions
---@return HttpResponse
function M.put(url, content, options)
    options = options or {}
    return internal.request("PUT", url, options, content)
end

---@param url string
---@param content string
---@param options? HttpOptions
---@return HttpResponse
function M.post(url, content, options)
    options = options or {}
    return internal.request("POST", url, options, content)
end

---@param url string
---@param form table @
---@param options? HttpOptions
---@return HttpResponse
function M.post_form(url, form, options)
    options = options or {}
    options.header = options.header or {}
    options.header["content-type"] = "application/x-www-form-urlencoded"
    for k, v in pairs(form) do
        form[k] = tostring(v)
    end
    return internal.request("POST", url, options, create_query_string(form))
end

local json_content_type = { ["Content-Type"] = "application/json" }

---@param url string
---@param data table @ table for encode to json
---@param options? HttpOptions
---@return HttpResponse
function M.post_json(url, data, options)
    options = options or {}
    if not options.headers then
        options.headers = json_content_type
    else
        if not options.headers['Content-Type'] then
            options.headers['Content-Type'] = "application/json"
        end
    end
    local response = internal.request("POST", url, options, json.encode(data))
    if response.status_code == 200 then
        response.body = json.decode(response.body)
    end
    return response
end

return M
