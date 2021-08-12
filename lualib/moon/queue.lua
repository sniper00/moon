local moon = require("moon")
local list = require("list")

local coroutine = coroutine

local function queue()
	local current_thread
	local ref = 0
	local thread_queue = list.new()

	local scope = setmetatable({}, { __close = function()
		ref = ref - 1
		if ref == 0 then
			current_thread = list.pop(thread_queue,1)
			if current_thread then
				moon.wakeup(current_thread)
			end
		end
	end})

	return function()
		local thread = coroutine.running()
		if current_thread and current_thread ~= thread then
			list.push(thread_queue, thread)
			coroutine.yield()
			assert(ref == 0)	-- current_thread == thread
		end
		current_thread = thread
		ref = ref + 1
		return scope
	end
end

return queue
