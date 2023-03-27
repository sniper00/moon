local moon = require "moon"
local socket = require "moon.socket"
local redisd = require "redisd"

local addr_center = 0

---@class client
local client = {
    id = moon.id,
    name = "",
    addr_room = 0,
    score = 0
}

---服务器内部命令
---@class user_command
local command = {}

---客户端命令
---@class client_command
local client_command = {}

function client_command.help()
    local help_string = [[

help                #帮助
login <name>        #登录
score               #查询当前积分
ready               #开始匹配
guess <number>      #猜数字[1-100]
quit                #退出]]
    command.send_to_client(help_string)
end

function client_command.login(name)
    if string.len(client.name)>0 then
        command.send_to_client("已经登录!")
        return
    end
    client.name = name
    client.score = redisd.call(moon.queryservice("db"), "HGET", client.name, "score") or 0
    command.send_to_client("登录成功, 当前积分:"..client.score)
    client.addr_room = moon.call("lua", addr_center, "online", client)
end

function client_command.score()
    if string.len(client.name)==0 then
        command.send_to_client("需要先登录!")
        return
    end
    command.send_to_client("当前积分:"..client.score)
end

function client_command.ready()
    if string.len(client.name)==0 then
        command.send_to_client("需要先登录!")
        return
    end
    moon.send("lua", addr_center, "ready", client)
end

function client_command.guess(num)
    if client.addr_room ==0 then
        command.send_to_client("没有在房间中!")
        return
    end
    moon.send("lua", client.addr_room, "guess", client.name, num)
end

function client_command.quit()
    if #client.name >0 then
        moon.send("lua", addr_center, "offline", client)
    end
    socket.close(client.fd)
    moon.quit()
end

-----------------------------------------------

function command.send_to_client(data)
    socket.write(client.fd, data.."\n>>")
end

function command.update_room(addr_room)
    client.addr_room = addr_room
end

function command.game_over(iswin)
    client.addr_room = 0
    if iswin then
        redisd.send(moon.queryservice("db"), "Hincrby", client.name, "score", 10)
        client.score = client.score + 10
        command.send_to_client("当前积分:"..client.score)
    else
        redisd.send(moon.queryservice("db"), "Hincrby", client.name, "score", 5)
        client.score = client.score + 5
        command.send_to_client("当前积分:"..client.score)
    end
end

function command.start(fd, timeout)
    socket.settimeout(fd, timeout)

    addr_center = moon.queryservice("center")
    socket.write(fd, "欢迎来带猜数字游戏, 输入'help'查看可用命令.\n")
    socket.write(fd, ">>")

    client.fd = fd

    while true do
        local data, err = socket.read(fd, "\n")
        if not data then
            client_command.quit()
            return
        end

        local req = {}
        for v in data:gmatch("%w+") do
            req[#req+1] = v
        end

        if not next(req) then
            command.send_to_client("error request format")
        else
            local fn = client_command[req[1]]
            if not fn then
                command.send_to_client("unknown command: "..req[1])
                client_command.help()
            else
                moon.async(function ()
                    fn(table.unpack(req,2))
                end)
            end
        end
    end
end

moon.dispatch("lua", function(sender, session, cmd, ...)
    local fn = command[cmd]
    if fn then
        moon.response("lua", sender, session, fn(...))
    else
        moon.error("unknown command", cmd, ...)
    end
end)
