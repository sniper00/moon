---create a queue based hash
---

local queue = {}

function queue.new()
    return { h = 1, t = 0 }
end

function queue.push(q, v)
    local t = q.t + 1
	q.t = t
	q[t] = v
end

function queue.front(q)
    if q.h > q.t then
        -- queue is empty
        q.h = 1
        q.t = 0
        return
    end
    local h = q.h
    return q[h]
end

function queue.pop(q)
    if q.h > q.t then
        -- queue is empty
        q.h = 1
        q.t = 0
        return
    end
    -- pop queue
    local h = q.h
    local v = q[h]
    q[h] = nil
    q.h = h + 1
    return v
end

function queue.size(q)
    local size = 0
    for k,_ in pairs(q) do
        if k~="h" and k~="t" then
            size = size + 1
        end
    end
    return size
end

return queue