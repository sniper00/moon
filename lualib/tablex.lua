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

return M