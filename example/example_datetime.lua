local moon = require("moon")

local datetime = require("moon.datetime")

print(datetime.year(),datetime.month(),datetime.day(),datetime.hour(),datetime.minutes(),datetime.seconds())
print("is_leap_year", datetime.is_leap_year())
print("localday", datetime.localday())

local t1 = 1578240000-1
local t2 = 1578240000

print(datetime.localday(t1), datetime.localday(t2))

moon.abort()