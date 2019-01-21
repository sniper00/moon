local moon = require("moon")
local aoi = require("aoi")

local function test(space)
	space:insert(1,40,0)
	space:insert(2,42,2)
	space:insert(3,-42,-42)
	space:insert(4,100,-42)

	local t = {}
	local version = 1
	for i=1,255 do
		version = space:query(1,20,10,version,t)
		if i == 100 then
			space:erase(2)
		elseif i==101 then
			space:insert(2,42,5)
		end
		for id,v in pairs(t) do
			if v == version then
				print("id: ", id, "enter view")
			elseif v~= (version-1) then
				t[id] = nil
				print("id: ", id, "leave view")
			end
		end
	end

	moon.abort()
end

local function run()
	local space = aoi.create(-250,-250, 500, 500)
	test(space)
	aoi.release(space)
end

moon.start(function()
	run()
end)

