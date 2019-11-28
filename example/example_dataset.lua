local moon= require("moon")
local dataset = require("dataset")

local Layer1 =
{
    field1 = {id = 1, type = dataset.DATA_TYPE_BASE},
    field2 = {id = 2, type = dataset.DATA_TYPE_BASE},
    test2 = {id = 3, type = dataset.DATA_TYPE_OBJ_ARRAY},
}

local Layer2 =
{
    field1 = {id = 1, type = dataset.DATA_TYPE_BASE},
	field2 = {id = 2, type = dataset.DATA_TYPE_BASE},
	field3 = {id = 3, type = dataset.DATA_TYPE_ARRAY},
}

moon.start(function()
	local root = dataset.object.new()
	root:open_record()
	root:set(Layer1.field1,123)
	root:set(Layer1.field2,456)
	local test2 = root:get(Layer1.test2)
	local o1 = test2:get(1)
	o1:set(Layer2.field1,678)
	o1:set(Layer2.field2,897)
	local f3 = o1:get(Layer2.field3)
	f3:set(1,1)
	f3:set(2,2)
	f3:set(3,3)
	f3:set(4,4)

	--another dataset
	local root2 = dataset.object.new()

    -- set by record
	local tmp = root2
	for _,op in pairs(root:record()) do
		if op[1] == dataset.OP_SET then
			tmp:set(op[2],op[3])
		elseif op[1] == dataset.OP_GET then
			tmp = tmp:get(op[2])
		end
	end
	root:clear_record()
	print_r(root2)

	root:get(Layer1.test2):get(1):get(Layer2.field3):set(1,99)

    -- update by record
	local tmp = root2
	for _,op in pairs(root:record()) do
		if op[1] == dataset.OP_SET then
			tmp:set(op[2],op[3])
		elseif op[1] == dataset.OP_GET then
			tmp = tmp:get(op[2])
		end
	end

	print_r(root2)
end)