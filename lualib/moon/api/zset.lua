error("DO NOT REQUIRE THIS FILE")

---@class zset
local zset = {}

---@param maxcount integer
---@param reverse? boolean @ Default sort the scores from high to low. if reverse is true: sort the scores from low to high.
---@return zset
function zset.new(maxcount, reverse) end

---Sort by score and timestamp. Time complexity O(logN)
--- Comapre function
--- ```cpp
--- if (score == val.score)
--- {
---     if (timestamp == val.timestamp)
---     {
---         return key < val.key;
---     }
---     return timestamp < val.timestamp;
--- }
--- return score > val.score;
--- ```
---@param key integer @unique id
---@param score integer
---@param timestamp integer
function zset:update(key, score, timestamp) end

--- Returns the rank of the key. Time complexity: O(log(N)).
--- The rank (or index) is 1-based, nil means key does not exist
---@param key integer @unique id
---@return integer
function zset:rank(key) end

--- Returns the score of the key. Time complexity: Constant.
---@param key integer @unique id
function zset:score(key) end

---@param key integer @unique id. Time complexity: Constant.
---@return boolean
function zset:has(key) end

--- Return the size of zset. Time complexity: Constant.
function zset:size() end

--- Clear the zset. Time complexity: Linear in the size of the zset.
function zset:clear() end

--- Removes the specified member ny key. Time complexity: O(log(N)).
---@param key integer @unique id
---@return integer @ Removed count
function zset:erase(key) end

--- Returns the specified range[start, stop] of keys by rank. The rank (or index) is 1-based
--- Time complexity: O(log(N)+M) with N being the number of elements in the sorted set and M the number of elements returned.
---@param start integer
---@param stop integer
---@param reverse? boolean @ Default false. If true: reverse the ordering
---@return integer[]?
function zset:range(start, stop, reverse) end

--- Returns key by rank, nil means does not exist
function zset:key_by_rank(rank) end

return zset