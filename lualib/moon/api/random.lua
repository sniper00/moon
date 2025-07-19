---@meta

error("DO NOT REQUIRE THIS FILE")

---@class random
---Random number generator class providing various random number generation functions
---Including integer range random, float range random, weighted random, etc.
local random = {}

---Generate a random integer within the specified range [min, max]
---@param min integer @ Minimum value (inclusive)
---@param max integer @ Maximum value (inclusive)
---@return integer @ Returns a random integer in range [min, max]
---@error Throws error when min > max
---@example
---local r = random.rand_range(1, 10) -- Returns random integer between 1 and 10
function random.rand_range(min, max) end

---Generate specified number of unique random integers within range [min, max]
---@param min integer @ Minimum value (inclusive)
---@param max integer @ Maximum value (inclusive)
---@param count integer @ Number of random integers to generate
---@return table @ Returns an array containing count unique random integers
---@error Throws error when count <= 0 or (max - min + 1) < count
---@example
---local result = random.rand_range_some(1, 10, 5) -- Returns 5 unique random numbers between 1 and 10
function random.rand_range_some(min, max, count) end

---Generate a random float within the specified range [min, max)
---@param min number @ Minimum value (inclusive)
---@param max number @ Maximum value (exclusive)
---@return number @ Returns a random float in range [min, max)
---@example
---local r = random.randf_range(0.0, 1.0) -- Returns random float between 0.0 and 1.0
function random.randf_range(min, max) end

---Return boolean based on probability
---@param percent number @ Probability value in range (0, 1.0]
---@return boolean @ Returns true or false based on the probability
---@note Always returns false when percent <= 0
---@example
---local result = random.randf_percent(0.3) -- 30% chance to return true
function random.randf_percent(percent) end

---Randomly select a value based on weights
---@param values integer[] @ Array of values to choose from
---@param weights integer[] @ Array of corresponding weights
---@return integer @ Returns a randomly selected value based on weights
---@error Throws error when values is empty, weights is empty, or their lengths don't match
---@note Returns 0 when all weights are 0
---@example
---local values = {1, 2, 3}
---local weights = {10, 20, 30} -- Weights are 10, 20, 30 respectively
---local result = random.rand_weight(values, weights) -- Select based on weights
function random.rand_weight(values, weights) end

---Randomly select specified number of values based on weights (without repetition)
---@param values integer[] @ Array of values to choose from
---@param weights integer[] @ Array of corresponding weights
---@param count integer @ Number of values to select
---@return table @ Returns an array containing count randomly selected values based on weights
---@error Throws error when values is empty, weights is empty, their lengths don't match, count < 0, or count > values length
---@note Returns empty array when all weights are 0
---@example
---local values = {1, 2, 3, 4}
---local weights = {10, 20, 30, 40}
---local result = random.rand_weight_some(values, weights, 2) -- Select 2 values based on weights
function random.rand_weight_some(values, weights, count) end

return random