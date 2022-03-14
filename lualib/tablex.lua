local M = {}

function M.remove(array, value)
    local len = #array
    for i=1, len do
        if array[i] == value then
            local v = array[i]
            if i ~= len then
                array[i] = array[len]
            end
            array[len] = nil
            return v
        end
    end
end

function M.remove_if(array, fn)
    local len = #array
    for i=1, len do
        if fn(array[i]) then
            local v = array[i]
            if i ~= len then
                array[i] = array[len]
            end
            array[len] = nil
            return v
        end
    end
end

function M.count_if(array, fn)
    local count = 0
    local len = #array
    for i=1, len do
        if fn(array[i]) then
            count = count + 1
        end
    end
    return count
end

---
--- Returns true if the table contains the specified value.
---@param t table
---@param value any
---@param fieldname string
function M.contains(t, value, fieldname)
    for k, v in pairs(t) do
        if not fieldname and v == value then
            return true
        elseif fieldname and v[fieldname] == value then
            return true
        end
    end
    return false
end

function M.contains_if(t, fn)
    for k, v in pairs(t) do
        if fn(v) then
            return true
        end
    end
    return false
end

return M