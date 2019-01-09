
---
--- Clamps a value between a minimum float and maximum float value.
--- e.
--- clamp(1.5, 5, 10) == 5
--- clamp(8.1, 5, 10) == 8.1
--- clamp(11, 5, 10) == 10
---@param value number
---@param minv number
---@param maxv number
function math.clamp(value, minv, maxv)
    if value < minv or value > maxv then
        value = value < minv and minv or maxv
    end
    return value
end
