---@meta

error("DO NOT REQUIRE THIS FILE")

--- High-performance ordered set with ranking capabilities
---
--- Efficient sorted set implementation optimized for leaderboards, time-series data,
--- and priority queues. Elements are sorted by score, timestamp, and key with
--- configurable ordering and automatic capacity management.
---
--- **Performance:**
--- - O(log N): insertion, deletion, rank queries
--- - O(1): score lookup, existence checks
--- - O(log N + M): range queries for M elements
---
--- **Sorting:**
--- - Primary: score (ascending/descending based on reverse flag)
--- - Secondary: timestamp (always ascending)
--- - Tertiary: key (always ascending for determinism)
---
--- **Use Cases:**
--- - Leaderboards, priority queues, rate limiting
--- - Cache management, tournament brackets
--- - Performance monitoring, time-series analysis
---
---@class zset
local zset = {}

--- Create a new ordered set
---
--- **Parameters:**
--- - `maxcount`: Maximum elements to store (use 2× for fluctuating scores)
--- - `reverse`: Sort order - false=descending (default), true=ascending
---
--- **Examples:**
--- ```lua
--- -- Leaderboard (descending scores)
--- local leaderboard = zset.new(1000, false)
--- 
--- -- Response time tracker (ascending - lower is better)
--- local latency_tracker = zset.new(5000, true)
--- 
--- -- Priority queue
--- local task_queue = zset.new(10000, false)
--- ```
---
---@param maxcount integer @ Maximum number of elements
---@param reverse? boolean @ Sort order: false=descending, true=ascending
---@return zset @ New ordered set instance
---@nodiscard
function zset.new(maxcount, reverse) end

--- Insert or update element (O(log N))
---
--- Adds new element or updates existing one. Automatically positions by sort criteria
--- and enforces capacity limits through eviction if necessary.
---
--- **Behavior:**
--- - New elements: inserted at correct position
--- - Existing elements: score/timestamp updated, position recalculated
--- - Capacity overflow: lowest-ranked element evicted
--- - Duplicate scores: sorted by timestamp, then key
---
--- **Examples:**
--- ```lua
--- -- Basic usage
--- leaderboard:update(player_id, score, timestamp)
--- 
--- -- Batch updates
--- for player_id, data in pairs(updates) do
---     leaderboard:update(player_id, data.score, data.timestamp)
--- end
--- ```
---
---@param key integer @ Unique element identifier
---@param score integer @ Sorting score value
---@param timestamp integer @ Timestamp for secondary sorting
function zset:update(key, score, timestamp) end

--- Get element's rank position (O(log N))
---
--- Returns 1-based rank of element in sorted order. Rank 1 = highest-ranked.
--- Returns nil if element doesn't exist.
---
--- **Examples:**
--- ```lua
--- local rank = leaderboard:rank(player_id)
--- if rank then
---     print("Player rank:", rank)
--- else
---     print("Player not found")
--- end
--- ```
---
---@param key integer @ Element identifier
---@return integer|nil @ 1-based rank position, or nil if not found
---@nodiscard
function zset:rank(key) end

--- Get element's score value (O(1))
---
--- Returns score associated with key. Returns 0 for non-existent keys.
---
--- **Examples:**
--- ```lua
--- local score = leaderboard:score(player_id)
--- if score > 0 then
---     print("Player score:", score)
--- end
--- ```
---
---@param key integer @ Element identifier
---@return integer @ Current score value, or 0 if not found
---@nodiscard
function zset:score(key) end

--- Check if element exists (O(1))
---
--- Performs constant-time existence check. More efficient than checking rank().
---
--- **Examples:**
--- ```lua
--- if leaderboard:has(player_id) then
---     -- Player exists in leaderboard
--- end
--- ```
---
---@param key integer @ Element identifier
---@return boolean @ True if element exists
---@nodiscard
function zset:has(key) end

--- Get current element count (O(1))
---
--- Returns number of elements currently in the set.
---
--- **Examples:**
--- ```lua
--- local count = leaderboard:size()
--- print("Total players:", count)
--- ```
---
---@return integer @ Current number of elements
---@nodiscard
function zset:size() end

--- Remove all elements (O(N))
---
--- Clears all elements from the set. Memory is deallocated but container
--- structure remains ready for reuse.
---
--- **Examples:**
--- ```lua
--- leaderboard:clear() -- Reset for new session
--- ```
---
function zset:clear() end

--- Remove specific element (O(log N))
---
--- Removes element by key. Returns number of elements removed (0 or 1).
---
--- **Examples:**
--- ```lua
--- local removed = leaderboard:erase(player_id)
--- if removed > 0 then
---     print("Player removed")
--- end
--- ```
---
---@param key integer @ Element identifier
---@return integer @ Number of elements removed (0 or 1)
function zset:erase(key) end

--- Get range of elements by rank (O(log N + M))
---
--- Returns array of keys for elements within rank range. Supports negative
--- indices counting from end.
---
--- **Index Rules:**
--- - Positive: 1-based from start (1 = best rank)
--- - Negative: count from end (-1 = worst rank)
--- - Out of bounds: automatically clamped
---
--- **Examples:**
--- ```lua
--- -- Top 10 players
--- local top10 = leaderboard:range(1, 10)
--- 
--- -- Bottom 5 players
--- local bottom5 = leaderboard:range(-5, -1)
--- 
--- -- Pagination
--- local page = leaderboard:range(start_rank, end_rank)
--- 
--- -- Reverse order within range
--- local reverse_range = leaderboard:range(1, 10, true)
--- ```
---
---@param start integer @ Starting rank (1-based, negative counts from end)
---@param stop integer @ Ending rank (1-based, negative counts from end, inclusive)
---@param reverse? boolean @ Reverse order within range (default: false)
---@return integer[] @ Array of element keys in rank order
---@nodiscard
function zset:range(start, stop, reverse) end

--- Get element key by rank position (O(log N))
---
--- Returns key of element at specified rank. Returns nil if rank out of bounds.
---
--- **Examples:**
--- ```lua
--- -- Get current leader
--- local leader_id = leaderboard:key_by_rank(1)
--- 
--- -- Get last place
--- local last_player = leaderboard:key_by_rank(leaderboard:size())
--- 
--- -- Random selection
--- local random_rank = math.random(1, leaderboard:size())
--- local random_player = leaderboard:key_by_rank(random_rank)
--- ```
---
---@param rank integer @ 1-based rank position (1 = best rank)
---@return integer|nil @ Element key at rank, or nil if out of bounds
---@nodiscard
function zset:key_by_rank(rank) end

return zset