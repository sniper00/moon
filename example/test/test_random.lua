local moon = require("moon")
---@type random
local random = require("random")
local test_assert = require("test_assert")

do
	local value = random.rand_range(3, 3)
	test_assert.equal(value, 3)

	for _ = 1, 100 do
		value = random.rand_range(1, 10)
		test_assert.assert(value >= 1 and value <= 10, "rand_range should stay within bounds")
	end

	local ok = pcall(random.rand_range, 10, 1)
	test_assert.assert(not ok, "rand_range min > max should fail")
end

do
	local values = random.rand_range_some(1, 5, 5)
	test_assert.equal(#values, 5)

	local seen = {}
	for i = 1, #values do
		local value = values[i]
		test_assert.assert(value >= 1 and value <= 5, "rand_range_some should stay within bounds")
		test_assert.assert(not seen[value], "rand_range_some should not repeat values")
		seen[value] = true
	end

	local ok = pcall(random.rand_range_some, 1, 3, 0)
	test_assert.assert(not ok, "rand_range_some count=0 should fail")

	ok = pcall(random.rand_range_some, 1, 3, 4)
	test_assert.assert(not ok, "rand_range_some count > range should fail")
end

do
	for _ = 1, 50 do
		local value = random.randf_range(2.5, 3.5)
		test_assert.assert(value >= 2.5 and value < 3.5, "randf_range should stay in [min, max)")
	end

	test_assert.equal(random.randf_percent(0), false)
	test_assert.equal(random.randf_percent(-1), false)
	test_assert.equal(random.randf_percent(2), true)
end

do
	local value = random.rand_weight({11, 12, 13}, {0, 0, 5})
	test_assert.equal(value, 13)
	test_assert.equal(random.rand_weight({1, 2}, {0, 0}), nil)

	local ok = pcall(random.rand_weight, {1, 2, 3}, {1, 2})
	test_assert.assert(not ok, "rand_weight mismatched table sizes should fail")

	ok = pcall(random.rand_weight, {}, {})
	test_assert.assert(not ok, "rand_weight empty tables should fail")
end

do
	local values = random.rand_weight_some({21, 22, 23}, {0, 0, 7}, 1)
	test_assert.equal(#values, 1)
	test_assert.equal(values[1], 23)

	values = random.rand_weight_some({1, 2, 3}, {1, 1, 1}, 0)
	test_assert.equal(#values, 0)

	test_assert.equal(random.rand_weight_some({1, 2}, {0, 0}, 1), nil)

	local ok = pcall(random.rand_weight_some, {1, 2, 3}, {1, 2}, 1)
	test_assert.assert(not ok, "rand_weight_some mismatched table sizes should fail")

	ok = pcall(random.rand_weight_some, {1, 2, 3}, {1, 2, 3}, -1)
	test_assert.assert(not ok, "rand_weight_some negative count should fail")

	ok = pcall(random.rand_weight_some, {1, 2, 3}, {1, 2, 3}, 4)
	test_assert.assert(not ok, "rand_weight_some count > size should fail")
end

moon.async(function()
	test_assert.success()
end)