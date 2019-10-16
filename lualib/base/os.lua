
function os.gettimezone()
    local now = os.time()
    return os.difftime(now, os.time(os.date("!*t", now))) / 3600
end

function os.gettime(date, utc)
    local t = os.time({
        year  = date[1],
        month = date[2],
        day   = date[3],
        hour  = date[4],
        min   = date[5],
        sec   = date[6],
    })
    if utc ~= false then
        local now = os.time()
        local offset = os.difftime(now, os.time(os.date("!*t", now)))
        t = t + offset
    end
    return t
end