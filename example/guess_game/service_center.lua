local moon = require("moon")

---@type client[]
local match_queue = {}

---@type table<string, boolean>
local match_state = {}

local user_rooms = {}

local max_player_number = 2

--简单的匹配策略
local function update_match()
    if #match_queue >= max_player_number then
        local addr_room = moon.new_service( {
            name = "room",
            file = "service_room.lua"
        })
        if addr_room == 0 then
            moon.error("create room failed!")
            return
        end

        ---@type client[]
        local ready_client = {}
        while #ready_client < max_player_number do
            local client = table.remove(match_queue, 1)
            if not client then
                break
            end
            local p = match_state[client.name]
            if p then
                match_state[client.name] = nil
                ready_client[#ready_client+1] = client
                user_rooms[client.name] = addr_room
            end
        end

        if #ready_client == max_player_number then
            moon.send("lua", addr_room, "start", ready_client)
        else
            for _, v in ipairs(ready_client) do
                user_rooms[v.name] = nil
                match_state[v.name] = true
                table.insert(match_queue, 1, v)
            end

            moon.kill(addr_room)
        end
    end
end

local command = {}

function command.ready(client)
    if user_rooms[client.name] and user_rooms[client.name] > 0 then
        moon.send("lua", client.id, "send_to_client", "已经在房间中")
        return
    end

    local p = match_state[client.name]
    if not p then
        match_state[client.name] = true
        match_queue[#match_queue+1] = client
        update_match()
    end
    moon.send("lua", client.id, "send_to_client", "匹配成功")
end

function command.online(client)
    local addr_room = user_rooms[client.name]
    if addr_room and addr_room > 0 then
        moon.send("lua", addr_room, "online", client)
        return addr_room
    end
    return 0
end

function command.offline(client)
    match_state[client.name] = nil
    local addr_room = user_rooms[client.name]
    if addr_room and addr_room > 0 then
        moon.send("lua", addr_room, "offline", client)
    end
end

function command.game_over(room_clients)
    for _, client in ipairs(room_clients) do
        user_rooms[client.name] = nil
    end
end

function command.shutdown()
    moon.quit()
    return true
end

moon.dispatch("lua", function (sender, session, cmd, ...)
    local fn = command[cmd]
    if fn then
        moon.response("lua", sender, session, fn(...))
    else
        moon.error("unknown command", cmd, ...)
    end
end)