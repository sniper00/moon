local moon = require("moon")
local aoi = require("aoi")

local AOI_WATCHER = 1
local AOI_MARHER = 2

local event_cache = {}
local function print_aoi_event(space)
	local count = space:update_event(event_cache)
	for i=1,count,3 do
        local watcher = event_cache[i]
        local marker = event_cache[i+1]
		local eventid = event_cache[i+2]
		if eventid == 1 then
			print(marker, "enter", watcher,"view")
		end

		if eventid == 2 then
			print(marker, "leave", watcher,"view")
		end
	end
end

local function aoi_insert(space,...)
	space:insert(...)
	print_aoi_event(space)
end

local function aoi_erase(space, id)
	space:erase(id)
	print_aoi_event(space)
end

local function test(space)
	aoi_insert(space, 1,40,0,20,10,0,AOI_WATCHER)
	aoi_insert(space, 2,42,2,0,0,0,AOI_MARHER)
	aoi_insert(space, 3,-42,-42,0,0,0,AOI_MARHER)
	aoi_insert(space, 4,100,-42,0,0,0,AOI_MARHER)

	aoi_erase(space, 2)
	aoi_insert(space,2,42,5,0,0,0,AOI_MARHER)

	moon.exit(-1)
end

local function run()
	local space = aoi.create(-256,-256, 512, 8)
	test(space)
	aoi.release(space)
end

run()

