local moon = require("moon")

---@class tm
local tm = {
    yeay = 0,
    month = 0,
    day = 0,
    hour = 0,
    min = 0,
    sec = 0,
    weekday = 0,
    yearday = 0
}

local _time = moon.time
local timezone = moon.timezone

---@type fun():tm
local _localtime = moon.localtime

---@type fun(integer)
local _dailytime = moon.dailytime

local SECONDS_ONE_DAY = 86400
local SECONDS_ONE_HOUR = 3600

---@class datetime
local datetime = {}

---@param time integer @utc时间,单位秒
---@return tm
function datetime.localtime(time)
    return _localtime(time)
end

---生成一个time时间所在天,0点时刻的utc time
---@param time integer @utc时间,单位秒
---@return integer
function datetime.dailytime(time)
    return _dailytime(time)
end

---获取utc时间总共经过多少天,常用于跨天判断
---@param time integer @utc时间,单位秒。如果为nil,则返回服务器时间计算的结果值
---@return integer
function datetime.localday(time)
    if not time then
        time = _time()
    end
    return ((time+timezone*SECONDS_ONE_HOUR)//SECONDS_ONE_DAY)
end

local localday = datetime.localday

---获取utc时间是否是闰年
---@param time integer @utc时间,单位秒
---@return boolean
function datetime.is_leap_year(time)
    local y = _localtime(time).yeay
    return (y % 4) == 0 and ((y % 100) ~= 0 or (y % 400) == 0);
end

---获取utc时间 time1 time2 是否是同一天
---@param time1 integer @utc时间,单位秒。
---@param time2 integer @utc时间,单位秒
---@return boolean
function datetime.is_same_day(time1, time2)
    return localday(time1) == localday(time2);
end

---判断是否同一周
---@param time1 integer @utc时间,单位秒。
---@param time2 integer @utc时间,单位秒
---@return boolean
function datetime.is_same_week(time1, time2)
    local pastDay = datetime.past_day(time1, time2)
    local lastWeekNum
    if (time1 > time2) then
        lastWeekNum = _localtime(time2).weekday
    else
        lastWeekNum = _localtime(time1).weekday
    end
    if lastWeekNum == 0 then
        lastWeekNum = 7
    end
    return ((pastDay + lastWeekNum) <= 7)
end

---判断是否同一月
---@param time1 integer @utc时间,单位秒
---@param time2 integer @utc时间,单位秒
---@return boolean
function datetime.is_same_month(time1, time2)
    local tm1 = _localtime(time1)
    local tm2 = _localtime(time2)

    if tm1.year == tm2.year and tm1.month==tm2.month then
        return true
    end
    return false
end

---获取两个utc时间相差几天，结果总是>=0
---@param time1 integer @utc时间,单位秒。
---@param time2 integer @utc时间,单位秒。
---@return boolean
function datetime.past_day(time1, time2)
    local d1 = localday(time1);
    local d2 = localday(time2);
    if d1 > d2 then
        return d1-d2
    else
        return d2 - d1
    end
end

---生成当前time,某个整点的 utc time
---@param time integer @utc时间,单位秒。
---@param hour integer @整点, 0-23
---@return integer
function datetime.make_hourly_time(time, hour)
    local t = _dailytime(time)
    return t + SECONDS_ONE_HOUR*hour;
end

---@param strtime string @ "2020/09/04 20:28:20"
---@return tm
function datetime.parse(strtime)
    local rep = "return {year=%1,month=%2,day=%3,hour=%4,min=%5,sec=%6}"
    local res = string.gsub(strtime, "(%d+)[/-](%d+)[/-](%d+) (%d+):(%d+):(%d+)", rep)
    return load(res)()
end

return datetime