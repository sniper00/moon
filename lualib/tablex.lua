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

---
--- Looks for an object within a table. Returns the value if found,
--- or nil if the object could not be found.
function M.find_if(array, fn)
    for k, v in pairs(array) do
        if fn(v) then
            return v
        end
    end
end

--- Creates a new table containing only the elements that pass the
--- given truth test.
--- 
---@generic T
---@param t table<integer, T>
---@param fn fun(item: T): boolean
---@return table<integer,T>
function M.filter(t, fn)
    local result = {}
    for k, v in pairs(t) do
        if fn(v) then
            table.insert(result, v)
        end
    end
    return result
end

return M