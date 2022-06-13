
---
--- This property shows the difference between 1 and the smallest floating point number which is greater than 1.
--- When we calculate the value of EPSILON property we found it as 2 to the power -52 (2^-52) which gives us a value of 2.2204460492503130808472633361816E-16.
math.epsilon = 2.22044604925031308e-16

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
        return value, true
    end
    return value, false
end

function math.cast_int(x)
    return math.floor(x + 0.0000001)
end

