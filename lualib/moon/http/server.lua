local moon = require("moon")
local buffer = require("buffer")
local fs = require("fs")
local socket = require("moon.socket")
local internal = require("moon.http.internal")

local assert = assert
local http_response = internal.http_response


local mimes = {
    [".txt"] = "text/plain",
    [".html"] = "text/html",
    [".css"] = "text/css",
    [".js"] = "text/javascript",
    [".json"] = "application/json",
    [".xml"] = "application/xml",
    [".jpg"] = "image/jpeg",
    [".jpeg"] = "image/jpeg",
    [".png"] = "image/png",
    [".gif"] = "image/gif",
    [".svg"] = "image/svg+xml",
    [".mp3"] = "audio/mpeg",
    [".ogg"] = "audio/ogg",
    [".wav"] = "audio/wav",
    [".mp4"] = "video/mp4",
    [".webm"] = "video/webm",
    [".pdf"] = "application/pdf",
    [".zip"] = "application/zip",
    [".woff"] = "application/font-woff",
    [".woff2"] = "application/font-woff2",
    [".eot"] = "application/vnd.ms-fontobject",
    [".ttf"] = "application/x-font-ttf",
    [".otf"] = "application/x-font-opentype",
}

local static_content

----------------------------------------------------------

local M = {}

M.header_max_len = 8192

M.content_max_len = false
--is enable keepalvie
M.keepalive = true

local routers = {}

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
                socket.write_then_close(fd, buffer.concat(response:tb()))
                return
            else
                socket.write(fd, buffer.concat(response:tb()))
                return true
            end
        end
    end

    local handler = routers[request.path]
    if handler then
        local ok, err = xpcall(handler, traceback, request, response)
        if not ok then
            if M.error then
                M.error(fd, err)
            else
                moon.error(err)
            end

            request.header["connection"] = "close"

            response.status_code = 500
            response:write_header("Content-Type", "text/plain")
            response:write("Server Internal Error")
        end
    else
        if M.error then
            M.error(fd, print_r(request, true))
        end
        response.status_code = 404
        response:write_header("Content-Type", "text/plain")
        response:write(string.format("Cannot %s %s", request.method, request.path))
    end

    if not M.keepalive or request.header["connection"] == "close" then
        response:write_header("Connection", "close")
        socket.write_then_close(fd, buffer.concat(response:tb()))
    else
        socket.write(fd, buffer.concat(response:tb()))
        return true
    end
end

-----------------------------------------------------------------

local listenfd

---@param timeout integer @read timeout in seconds
---@param pre? string @first request prefix data, for convert a tcp request to http request
function M.start(fd, timeout, pre)
    socket.settimeout(fd, timeout)
    moon.async(function()
        while true do
            local request = internal.read_request(fd, pre, M)
            if pre then
                pre = nil
            end

            if request.error then
                local res = http_response.new()
                res.status_code = 400
                socket.write_then_close(fd, buffer.concat(res:tb()))
                if M.error then
                    M.error(fd, request.error)
                elseif not request.network_error then
                    moon.error("HTTP_SERVER_ERROR: " .. request.error)
                end
                return
            end

            if not request_handler(fd, request) then
                return
            end
        end
    end) --async
end

---@param host string @ ip address
---@param port integer @ port
---@param timeout? integer @read timeout in seconds
function M.listen(host, port, timeout)
    assert(not listenfd, "http server can only listen port once.")
    listenfd = socket.listen(host, port, moon.PTYPE_SOCKET_TCP)
    assert(listenfd>0, "Http server listen failed.")
    timeout = timeout or 0
    moon.async(function()
        while true do
            local fd, err = socket.accept(listenfd, moon.id)
            if not fd then
                moon.sleep(1000)
                print("httpserver accept", err)
            else
                M.start(fd, timeout)
            end
        end --while
    end)
end

---@param path string
---@param cb fun(request: HttpRequest, response: HttpResponse)
function M.on(path, cb)
    routers[path] = cb
end

local function read_asset(file, mime)
    mime = mime or mimes[fs.ext(file)]
    return {
        mime = mime or fs.ext(file),
        bin = io.readfile(file)
    }
end

function M.static(dir, showdebug)
    static_content = {}
    dir = fs.abspath(dir)
    local res = fs.listdir(dir, 100)
    for _, v in ipairs(res) do
        local pos = string.find(v, dir, 1, true)
        local src = string.sub(v, pos + #dir)
        src = string.gsub(src, "\\", "/")
        if string.sub(src, 1, 1) ~= "/" then
            src = "/" .. src
        end

        if not fs.isdir(v) then
            static_content[src] = read_asset(v)
            if showdebug then
                print("load static file:", src)
            end
        else
            local index_html = fs.join(v, "index.html")
            if fs.exists(index_html) then
                static_content[src] = read_asset(index_html)
                static_content[src .. "/"] = static_content[src]
                if showdebug then
                    print("load static index:", src)
                    print("load static index:", src .. "/")
                end
            end
        end
    end

    if fs.exists(fs.join(dir, "index.html")) then
        static_content["/"] = read_asset(fs.join(dir, "index.html"))
    end
end

return M
