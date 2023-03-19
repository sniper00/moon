local moon = require("moon")
local random = require("random")
local tablex = require("tablex")

local room_clients = {}

local function broadcast(data, exclude_id)
    for _, client in ipairs(room_clients) do
        if not exclude_id or client.id ~= exclude_id then
            moon.send("lua", client.id, "send_to_client", data)
        end
    end
end

local function send_to_client(client, data)
    moon.send("lua", client.id, "send_to_client", data)
end

---@class room_command
local command = {}

local random_num = random.rand_range(1,100)

print("guess num is", random_num)

local min_guess = 1
local max_guess = 100

function command.start(ready_client)
    room_clients = ready_client
    local names = {}
    for _, client in ipairs(ready_client) do
        moon.send("lua", client.id, "update_room", moon.id)
        names[#names+1] = client.name
    end
    broadcast("游戏开始, 欢迎 "..table.concat(names,',').." 进入游戏房间"..("现在的区间是[%d-%d]"):format(min_guess, max_guess));
    return true
end

function command.offline(client)
    tablex.remove_if(room_clients, function (v)
        return v.id == client.id
    end)
    broadcast(client.name.." 离开了房间")
end

function command.online(client)
    broadcast(client.name.." 进入了房间")
    room_clients[#room_clients+1] = client
    send_to_client(client, ("现在的区间是[%d-%d]"):format(min_guess, max_guess))
end

function command.guess(name, num)
    num = math.tointeger(num)
    if not num then
        send_to_client("无效的数字格式!")
        return
    end

    if random_num == num then
        broadcast(name.." 猜测成功, 游戏结束，请开始匹配新的游戏。")
        for _, client in ipairs(room_clients) do
            moon.send("lua", client.id, "game_over", client.name == name)
        end
        moon.send("lua", moon.queryservice("center"), "game_over", room_clients)
        moon.quit()
    else
        if num> min_guess and num < max_guess then
            if random_num > num then min_guess = num end
            if random_num < num then max_guess = num end
        end
        broadcast((name.." 猜测失败, 现在的区间是[%d-%d]"):format(min_guess, max_guess))
    end
end

moon.dispatch("lua", function (sender, session, cmd, ...)
    local fn = command[cmd]
    if fn then
        moon.response("lua", sender, session, fn(...))
    else
        moon.error("unknown command", cmd, ...)
    end
end)