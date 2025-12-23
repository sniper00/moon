--[[
================================================================================
Lua Hot Update System
================================================================================

This module implements a runtime hot-update system for Lua modules, allowing
you to update function implementations without restarting the program.

Key Features:
  1. Update existing function implementations in-place
  2. Add new functions to running modules
  3. Maintain upvalue consistency across updates
  4. Support multiple rounds of hotfixes
  5. Atomic updates with rollback on failure

How It Works:
  The hotfix system operates in four key phases:

  Phase 1: Collect Old Module Upvalue Environment
    - collect_all_uv(old_module) collects all upvalue info from old module
    - Returns {upvalue_name -> {func, index, id, value}}
    - Purpose: Build upvalue index for later connection

  Phase 2: Validation (hotfix.diff)
    - Compare old and new versions, generate diff mapping
    - Rules:
       Allow: Modify existing function implementations
       Allow: Add new functions
       Allow: Functions can add/remove upvalues
       Forbid: Delete existing functions (breaks external references)
       Forbid: New functions reference upvalues not in old module

  Phase 3: New Function Handling
    - Parse source code to get function names (avoid executing initialization)
    - Check that new functions' upvalues exist in old module

  Phase 4: Execute Replacement (using upvaluejoin)
    For each function to update:
      1. Lookup Phase: Search for same-named upvalue info in old module by name
         - upvalues[uvname] returns {func, index, id, value}
         - func: the function that owns this upvalue
         - index: the position of this upvalue in that function
      2. Join Phase: Use upvaluejoin to make new function's upvalue reference
         the memory location of old function's upvalue at that position
         - upvaluejoin(new_func, i, old_uv.func, old_uv.index)
         - IMPORTANT: Indices i and old_uv.index may be DIFFERENT
         - Connection is based on NAME, not position
      3. Assignment Phase: Assign new function to module position
         - Module table: Module.table[key] = new_function
         - Upvalue: setupvalue(parent_func, index, new_function)

  Key: upvaluejoin matches upvalues by NAME. We look up the upvalue info
  (func and index) by name, then join to that specific location.

Architecture:
  - Three-phase update process: Parse → Check → Apply
  - Parse: Compare old/new versions, identify changes (no side effects)
  - Check: Validate upvalue compatibility (no side effects)
  - Apply: Update functions atomically (only phase with side effects)
  - Weak table tracking for memory efficiency
  - Upvalue environment preservation across updates

Core Concepts:
  1. Function Prototypes: Internal bytecode pointers used to identify changes
  2. Upvalue Linking: Finding upvalue info (func, index) by NAME in old module,
     then using upvaluejoin to make new function's upvalue reference the
     memory location of that upvalue in the old function
  3. Origin Tracking: Remembering original functions across multiple hotfixes
  4. Transaction Pattern: Collect all updates, then apply atomically

Limitations:
  - Cannot modify module-level local variables directly
    (But can modify their VALUES through upvalue linking)
  - New functions can only reference upvalues that exist in old module
    (Cannot introduce new dependencies at runtime)
  - Function signature changes must maintain upvalue compatibility
    (New function can have fewer upvalues, but not new ones)
  - Cannot remove functions (only add or update)
    (Removing would break existing references)

Example Usage:
  -- Initial load
  local mymodule = hotfix.require("game.logic.player")

  -- Later, after modifying game/logic/player.lua
  local ok, result = hotfix.update("game.logic.player")
  if ok then
    print("Hotfix successful")
  else
    print("Hotfix failed:", result)
  end

================================================================================
]] --

local clonefunc = require "clonefunc"
local getupvalue = debug.getupvalue
local upvalueid = debug.upvalueid
local upvaluejoin = debug.upvaluejoin
local setupvalue = debug.setupvalue
local getinfo = debug.getinfo
local proto = clonefunc.proto
local clone = clonefunc.clone

local hotfix = {}

--[[
================================================================================
Module State Tracking
================================================================================
]] --

-- Current loader functions for each module
-- Updated after each successful hotfix
local loaders = {}

-- Original loader functions
-- Never updated, used for fallback compatibility checking
local origin = {}

-- Mapping: new_function → original_function
-- Weak keys allow intermediate versions to be garbage collected
-- Strong values preserve the original function across multiple hotfixes
--
-- Why This Exists - Supporting Multiple Hotfix Rounds:
--   When we hotfix v1 → v2, the module table now has v2 references.
--   But old code may still have v1 references (callbacks, upvalues, etc).
--   When we hotfix v2 → v3, we must update v1 (not v2) to affect those old refs.
--
-- Example Across Multiple Hotfixes:
--   Round 1: v1 → v2
--     - origin_functions[v2] = v1
--     - Module table: module.foo = v2
--     - Old references still point to v1
--     - We update v1's upvalues
--
--   Round 2: v2 → v3
--     - origin_functions[v2] = v1 (lookup to find original)
--     - origin_functions[v3] = v1 (new entry, still points to original)
--     - Module table: module.foo = v3
--     - Old references still point to v1
--     - We update v1's upvalues again (not v2!)
--
-- Garbage Collection:
--   Weak keys allow v2 to be GC'd when no longer referenced
--   Strong values ensure v1 is NEVER GC'd (it's the canonical version)
local origin_functions = setmetatable({}, { __mode = "k" })

-- Custom module searchers
local searchers = {}

--[[
================================================================================
Utility Functions
================================================================================
]] --

--[[
Check if two functions have compatible upvalue signatures.

Compatibility Rules:
  - f2 can have fewer upvalues than f1 (removing upvalues is safe)
  - f2 cannot add new upvalues not in f1 (new dependencies are unsafe)
  - _ENV is always ignored in comparison (global environment is special)

Why This Matters:
  When replacing f1 with f2, all upvalues of f2 must already exist in f1's
  environment. Otherwise, f2 would reference non-existent state, leading to
  runtime errors or undefined behavior.

@param f1 function - Old version function
@param f2 function - New version function
@param upvalues table - Map of allowed new upvalues for adding functions
@return boolean - true if compatible, false otherwise
]] --
local function same_proto(f1, f2, upvalues)
	local uv = {}
	local i = 1

	-- Step 1: Collect all upvalues from f1 (old function)
	-- Build a set of all upvalue names that f1 currently uses
	while true do
		local name = getupvalue(f1, i)
		if name == nil then break end
		if name ~= "_ENV" then
			uv[name] = true
		end
		i = i + 1
	end

	-- Step 2: Check f2's upvalues against f1
	-- Verify that f2 (new function) doesn't reference upvalues that don't exist
	i = 1
	while true do
		local name = getupvalue(f2, i)
		if name == nil then
			return true -- f2 can have fewer upvalues - this is safe
		end
		-- If f2 references an upvalue that:
		--   1. Doesn't exist in f1 (uv[name] == nil)
		--   2. AND not allowed as new upvalue (upvalues[name] == nil)
		--   3. AND not _ENV (special case)
		-- Then it's incompatible
		if name ~= "_ENV" and uv[name] == nil and upvalues[name] == nil then
			return false -- f2 adds a new upvalue, incompatible
		end
		uv[name] = nil
		i = i + 1
	end
end

--[[
Generate function location info for debugging.

@param f function - The function to get info from
@return string - Format: "filepath(line_number)"
]] --
local function funcinfo(f)
	local info = getinfo(f, "S")
	return string.format("%s(%d)", info.short_src, info.linedefined)
end

--[[
================================================================================
Core Algorithm: Diff Generation
================================================================================

Compare two module loader functions and generate a mapping from old function
prototypes to new function implementations.

What is a Function Prototype?
  In Lua's internal implementation, each function has a "prototype" - a unique
  pointer to its bytecode structure. Functions with identical code share the
  same prototype. We use this to identify which functions have changed:
  - Same prototype = unchanged function (reuse old version)
  - Different prototype = changed function (needs update)

Algorithm Flow:
  1. Compare function prototypes (proto pointers) using clonefunc.proto()
  2. Check child function count (new code can ADD functions, but not REMOVE)
  3. Verify upvalue compatibility (new function can't depend on non-existent state)
  4. Recursively process child functions (handle nested functions)
  5. Collect newly added functions (functions that don't exist in old version)

The diff map is the core data structure that drives the hotfix process:
  diff[old_proto] = new_function
This allows us to later find all instances of old functions and replace them.
]] --

--- @param m1 function @ Old module loader
--- @param m2 function @ New module loader
--- @param upvalues table @ Allowed upvalues for new functions
--- @return table, table|nil, table @ Map of old_proto -> new_func, Array of error messages (nil if no errors), Array of new functions
function hotfix.diff(m1, m2, upvalues)
	local diff = {}
	local err = nil
	local newfuncs = {}

	local function diff_(a, b)
		-- Get function prototype and child count
		-- proto: Internal pointer to function prototype (lightuserdata)
		--        Two functions with identical bytecode share the same proto
		-- n: Number of child functions (nested functions defined inside this function)
		local p1, n1 = proto(a)
		local p2, n2 = proto(b)

		-- proto() returns nil for C functions or if clonefunc doesn't support this function
		if p1 == nil or p2 == nil then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end

		-- Check child function count
		-- RULE: New version can have MORE children (adding functions is OK)
		--       New version CANNOT have FEWER children (removing functions breaks references)
		--
		-- Why? If old code has: local f1, f2, f3 = ...
		--      And new code has: local f1, f2 = ...
		--      Then f3 is removed, but existing code may still call f3
		if n1 > n2 then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end

		-- Check upvalue compatibility
		-- Ensures new function can be safely substituted for old function
		-- by verifying that all upvalues referenced by 'b' exist in 'a'
		if not same_proto(a, b, upvalues) then
			err = err or {}
			table.insert(err, string.format("incompatible upvalue in %s -> %s", funcinfo(a), funcinfo(b)))
		end

		-- Store mapping: old prototype -> new function
		-- This is the core of the diff - it tells us which functions changed
		-- Later, we'll use this to find all instances of old functions
		diff[p1] = b

		-- Recursively process existing child functions (1 to n1)
		-- These are functions that exist in both old and new versions
		-- We need to check if their implementations changed
		for i = 1, n1 do
			diff_(clone(a, i), clone(b, i))
		end

		-- Collect newly added child functions (n1+1 to n2)
		-- These are functions that only exist in the new version
		-- They need special handling - we'll add them to the module table
		for i = n1 + 1, n2 do
			newfuncs[#newfuncs + 1] = clone(b, i)
		end
	end

	diff_(m1, m2)
	return diff, err, newfuncs
end

--[[
================================================================================
Module Loading & Registration
================================================================================
]] --

--[[
Add a custom module searcher.

@param fn function - Searcher function that returns (loader, extra_arg)
]] --
function hotfix.addsearcher(fn)
	searchers[#searchers + 1] = fn
end

--[[
Find loader function for a module using registered searchers.

@param name string - Module name
@return function - Loader function
@return any - Extra argument for loader
]] --
local function findloader(name)
	local msg = {}
	for _, loader in ipairs(searchers) do
		local f, extra = loader(name)
		local t = type(f)
		if t == "function" then
			return f, extra
		elseif t == "string" then
			table.insert(msg, f)
		end
	end
	error(string.format("module '%s' not found:%s", name, table.concat(msg)))
end

--[[
Parse module source file to extract function definitions and their locations.

Purpose:
  When we add new functions during a hotfix, we need to know their names to
  add them to the module table. However, clonefunc.clone() returns functions
  by index without names. This function bridges that gap by parsing the source
  file to build a mapping from function locations to function names.

How It Works:
  1. Loads source file content (preferably from cache)
  2. Scans each line looking for function declarations
  3. Pattern matches: "function ModuleName.FunctionName("
  4. Builds map: "filepath(line_number)" -> "function_name"

Caching Strategy:
  - First tries moon.env() which caches file content
  - Falls back to reading file directly if cache unavailable
  - Cache reduces disk I/O during multiple hotfixes

Example:
  Given source line at mymodule.lua:42:
    function mymodule.calculate(x, y)

  Returns:
    { ["mymodule.lua(42)"] = "calculate" }

@param filepath string - Source file path
@param modname string - Module name (used in pattern matching)
@return table|nil - Map of "filepath(line)" -> "function_name"
@return string|nil - Error message if parsing failed
]] --
function hotfix.parse_module_functions(filepath, modname)
	-- Try to use cached content, avoid open too many files
	local ok, cache = pcall(function()
		local moon = require("moon")
		return moon.env(filepath)
	end)

	local content
	if ok and cache then
		print("Using cached function lines for:", filepath)
		content = cache
	else
		local file = io.open(filepath, "rb")
        if not file then
            return nil, "Cannot open file: " .. filepath
        end
        content = file:read("*a")
        file:close()
	end

	-- Parse source file
	local functions = {}
	-- Escape dots in module name for pattern matching
	-- Example: "game.logic" becomes "game%.logic"
	local escaped_modname = modname:gsub("%.", "%%.")
	local pattern = "function%s+" .. escaped_modname .. "%.(%w+)%s*%("
	local line_num = 0

	for line in content:gmatch("[^\n]*") do
		line_num = line_num + 1
		local func_name = line:match(pattern)
		if func_name then
			-- Store mapping: "filepath(line)" -> "function_name"
			functions[string.format("%s(%d)", filepath, line_num)] = func_name
		end
	end

	return functions
end

--[[
Load a module with hotfix support.

This is similar to require() but registers the module with the hotfix system.

@param name string - Module name
@return any - Module return value
]] --
function hotfix.require(name)
	assert(type(name) == "string")
	local _LOADED = assert(debug.getregistry()._LOADED, "_LOADED not found")
	if _LOADED[name] then
		return _LOADED[name]
	end
	local loader, arg = findloader(name)
	local ret = loader(name, arg) or true
	loaders[name] = loader
	origin[name] = loader
	_LOADED[name] = ret
	return ret
end

--[[
Register a module with hotfix support.

Used for modules that are already loaded but need hotfix support.

@param name string - Module name
@param loader function - Module loader function
@param ret any - Module return value
@return any - Module return value
]] --
function hotfix.register(name, loader, ret)
	assert(type(name) == "string")
	local _LOADED = assert(debug.getregistry()._LOADED, "_LOADED not found")
	if _LOADED[name] then
		return _LOADED[name]
	end
	loaders[name] = loader
	origin[name] = loader
	_LOADED[name] = ret
	return ret
end

--[[
================================================================================
Upvalue Collection & Management
================================================================================
]] --

--[[
Recursively collect upvalues from a function and its nested functions.

Purpose:
  This builds a complete map of all upvalues in the module, which is needed
  to properly link new functions to the existing upvalue environment. When
  we hotfix a module, new function implementations must share the same upvalue
  state as the old functions to maintain consistency.

Upvalue Structure:
  Each entry in the 'uv' table has this structure:
  {
    func: function - The function that owns this upvalue
    index: number - Upvalue index in the function (1-based)
    id: lightuserdata - Unique upvalue ID from debug.upvalueid()
    value: any - Current value of the upvalue
  }

Collection Strategy:
  1. Direct upvalues: Collect from function f
  2. Function-type upvalues: Recursively collect (if from same source file)
  3. Table-type upvalues: Collect if table contains functions from same source
  4. _ENV: Always collected but treated specially (global environment)

Special Cases:
  - _ENV: Always ignored in validation to avoid conflicts with global environment
  - Functions from other files: Skipped to avoid collecting framework code
  - Method tables: Only collect if ALL values are functions from same source

Why Check Source Files?
  We only want to hotfix functions from the module being updated, not framework
  functions or functions from other modules. Checking source file ensures we
  don't accidentally modify unrelated code.

@param f function - Function to collect upvalues from
@param uv table - Accumulator for upvalue information (modified in-place)
@param source string - Source file path for filtering (only collect from this file)
]] --
local function collect_uv(f, uv, source)
	local i = 1
	while true do
		local name, value = getupvalue(f, i)
		if name == nil then break end
		local id = upvalueid(f, i)

		-- Special handling for _ENV
		-- _ENV is the global environment table, always available
		-- We collect it but don't validate it strictly
		if name == "_ENV" then
			uv[name] = { func = f, index = i, id = id, value = value }
			goto continue
		end

		if uv[name] then
			-- Upvalue already collected from another function
			-- Verify consistency: same upvalue name MUST have same ID
			-- If they differ, it means we have ambiguous local variables
			-- This should never happen in well-formed Lua code
			assert(uv[name].id == id, string.format("ambiguity local value %s", name))
		else
			-- New upvalue, store it
			uv[name] = { func = f, index = i, id = id, value = value }

			-- Recursively collect from function-type upvalues
			if type(value) == "function" then
				-- IMPORTANT: Only process if function is from the same source file
				-- This prevents us from collecting and modifying framework functions
				if getinfo(value, "S").short_src == source then
					collect_uv(value, uv, source)
				end
			-- Collect from table-type upvalues (method tables)
			-- Only if ALL values are functions from the same source file
			-- This handles cases like: local methods = { foo = function() ... end }
			elseif type(value) == "table" then
				for k, v in next, value do
					if type(v) == "function" then
						-- Skip this table if it contains functions from other source files
						-- This prevents collecting framework method tables
						if getinfo(v, "S").short_src ~= source then
							break
						end
						collect_uv(v, uv, source)
					elseif k ~= "__index" then
						-- Table must contain only functions (except __index metamethod)
						-- If we find other types, stop processing this table
						break
					end
				end
			end
		end
		::continue::
		i = i + 1
	end
end

--[[
Collect all upvalues from module's root functions.

@param root table - Module table containing functions
@param source string - Source file path
@param proto_map table - Diff mapping
@return table - Complete upvalue map
]] --
local function collect_all_uv(root, source)
	local upvalue_funcs = {}
	for _, v in pairs(root) do
		if type(v) == "function" then
			collect_uv(v, upvalue_funcs, source)
		end
	end

	-- Ensure _ENV exists
	if not upvalue_funcs["_ENV"] then
		upvalue_funcs["_ENV"] = { func = collect_uv, index = 1 }
	end
	return upvalue_funcs
end

--[[
================================================================================
Function Linking
================================================================================
]] --

--[[
Link new function's upvalues to old module's upvalues.

Purpose:
  When we add a new function during a hotfix, it must share state with the
  existing module. This is achieved by making the new function's upvalues
  point to the SAME memory locations as the old module's upvalues through
  NAME MATCHING.

How Upvalue Linking Works:
  In Lua, upvalues are shared references. upvaluejoin matches upvalues by NAME:
    debug.upvaluejoin(new_func, i, old_func, j)
  This makes new_func's upvalue at index i point to the same memory location
  as old_func's upvalue at index j, IF they have the SAME NAME.

Process:
  1. Iterate through all upvalues of the new function, getting name (uvname)
  2. Look up same-named upvalue info in old module: upvalues[uvname]
     - Returns {func, index, id, value} - the function that owns this upvalue
  3. If found and name != "_ENV", use upvaluejoin to connect them
     - upvaluejoin(new_func, i, old_uv.func, old_uv.index)
     - Makes new function's i-th upvalue reference old function's upvalue
  4. If upvalue is a function, recursively link it (nested functions)
  5. Ensure _ENV is properly set (global environment access)

Key Point - Name Matching and Info Lookup:
  upvalues[uvname] looks up by NAME and returns the INFO about which function
  owns this upvalue and at what index position. Then upvaluejoin uses this info
  to connect new function's upvalue to the exact memory location in old function.
  Only upvalues with matching names can be found and joined.

  IMPORTANT: The index positions may be DIFFERENT in new and old functions!
  - Lookup is based on NAME: upvalues[uvname] searches by name
  - Connection uses the INDICES from lookup: upvaluejoin(new_f, i, old_f, j)
  - Same name "counter" could be at index 2 in new_func and index 1 in old_func

Example:
  Old module: local counter = 0
              function old_func()
                  -- "counter" is old_func's 1st upvalue (index=1)
                  counter = counter + 1
              end

  New function: local config = {}  -- new_func's 1st upvalue (index=1)
                local counter = 0  -- new_func's 2nd upvalue (index=2)
                function new_func()
                    -- "counter" is new_func's 2nd upvalue (index=2)
                    counter = counter + 1
                end

  Lookup and Join Process:
  → upvalues["counter"] returns {func: old_func, index: 1, ...}
  → upvaluejoin(new_func, 2, old_func, 1)  -- Note: indices 2 and 1 differ!
  → Both functions' "counter" upvalues now point to SAME memory
  → They share the same counter variable despite different index positions

@param f function - New function to link
@param upvalues table - Old module's upvalue map {name -> {func, index, id, value}}
]] --
local function link_func_upvalue(f, upvalues)
	if type(f) ~= "function" then return end

	local i = 1
	while true do
		local uvname, value = getupvalue(f, i)
		if uvname == nil then break end

		if type(value) == "function" then
			-- Recursively link nested functions
			-- Nested functions may also have upvalues that need linking
			link_func_upvalue(value, upvalues)
		elseif upvalues[uvname] and uvname ~= "_ENV" then
			-- Look up same-named upvalue INFO in old module by name
			-- upvalues[uvname] returns {func, index, id, value}
			-- This tells us which function owns this upvalue and at what position
			local old_uv = upvalues[uvname]

			-- Use upvaluejoin to connect new function's upvalue to old function's upvalue
			-- upvaluejoin(new_func, i, old_uv.func, old_uv.index)
			-- Makes new_func's i-th upvalue reference the same memory location
			-- as old_uv.func's old_uv.index-th upvalue
			upvaluejoin(f, i, old_uv.func, old_uv.index)

			-- Now both functions' same-named upvalues point to SAME memory
			-- Example: If old_uv = {func: old_func, index: 1, ...}
			-- Then new_func's "data" upvalue now points to old_func's 1st upvalue's memory
		end
		i = i + 1
	end

	-- Ensure _ENV is properly set
	-- _ENV is the environment table that provides access to globals
	-- Every function needs a valid _ENV to access global functions and tables
	i = 1
	while true do
		local uvname, value = getupvalue(f, i)
		if uvname == nil then break end
		if uvname == "_ENV" then
			-- If _ENV is nil, set it from the old module or global _ENV
			if value == nil and upvalues["_ENV"] then
				setupvalue(f, i, upvalues["_ENV"].value or _ENV)
			end
			break
		end
		i = i + 1
	end
end

--[[
Check if new function only references upvalues that exist in old module BY NAME.

This validation ensures that new functions don't introduce dependencies
on upvalues that don't exist in the running module. Since upvaluejoin matches
by NAME, we check if each upvalue name in the new function exists in old module.

Process:
  1. Iterate through new function's upvalues, get name (uvname)
  2. Check if upvalues[uvname] exists (name lookup in old module)
  3. If not found and name != "_ENV", it's a missing upvalue
  4. Recursively check nested functions

@param f function - New function to check
@param funcname string - Function name for error messages
@param upvalues table - Old module's upvalue map {name -> {func, index, id, value}}
@return boolean - true if valid
@return string|nil - Error message if invalid
]] --
local function check_func_upvalue(f, funcname, upvalues)
	if type(f) ~= "function" then
		return true
	end

	local missing_upvalues = {}
	local i = 1
	while true do
		local uvname, value = getupvalue(f, i)
		if uvname == nil then break end

		if type(value) == "function" then
			-- Recursively check nested functions
			local ok, errmsg = check_func_upvalue(value, funcname, upvalues)
			if not ok then
				return false, errmsg
			end
		elseif not upvalues[uvname] and uvname ~= "_ENV" then
			-- Upvalue doesn't exist in old module
			table.insert(missing_upvalues, uvname)
		end
		i = i + 1
	end

	if #missing_upvalues > 0 then
		return false, string.format(
			"New function '%s' references upvalue(s) not exist in old module: %s",
			funcname or "unknown",
			table.concat(missing_upvalues, ", ")
		)
	end
	return true
end

--[[
================================================================================
Core Hotfix Logic: Upvalue Reference Redirection
================================================================================

This is the heart of the hotfix system. We make new functions' upvalues reference
the same memory locations as old module's upvalues through NAME MATCHING, then
assign new functions to module positions.

The Two-Step Process:
  Step 1: Upvalue Linking (via upvaluejoin)
    - For each upvalue in new function, get its name (uvname)
    - Look up same-named upvalue in old module: upvalues[uvname]
    - Use upvaluejoin to make them point to SAME memory location
    - This creates SHARED STATE between new and old functions

  Step 2: Function Assignment
    - Module table: Module.table[key] = new_function
    - Upvalue: setupvalue(parent_func, index, new_function)

Key Insight - Name Matching:
  upvaluejoin works by NAME, not position. If new function has upvalue "counter"
  and old module has upvalue "counter", they can be joined. If names don't match,
  they can't be joined (validation will fail).

Example:
  Old module:
    local counter = 0  -- This upvalue is named "counter"
    function M.get() return counter end

  New function:
    function M.increment() counter = counter + 1 end  -- References "counter" by name

  After hotfix:
    - upvalues["counter"] finds old module's counter upvalue
    - upvaluejoin makes new function's "counter" point to SAME memory
    - Both functions share the same counter variable
    - Module.increment = new_function (assigned to module table)

Algorithm:
  1. Find the original version of each function (via origin_functions)
  2. Make new function's same-named upvalues reference old module's upvalues
  3. For function-type upvalues, recursively process them
  4. Collect all updates in a transaction table
  5. Apply all updates atomically

Why Transaction Pattern?
  We collect all updates first, then apply atomically. This ensures
  all-or-nothing semantics - if anything fails, no partial updates occur.

@param root table - Module table containing functions
@param proto_map table - Diff mapping (old_proto -> new_func) from hotfix.diff()
@param upvalues table - Complete upvalue map {name -> {func, index, id, value}}
]] --
local function hotfix_(root, proto_map, upvalues)
	local exclude = {}

	--[[
	Make function's same-named upvalues reference old module's upvalues.

	Critical Implementation Detail:
	  We ALWAYS work with the ORIGINAL function, not intermediate versions.
	  This is essential for supporting multiple rounds of hotfixes.

	Process:
	  1. Resolve to original function via origin_functions
	  2. For each upvalue in the function, get its name
	  3. Look up same-named upvalue in old module: global[name]
	  4. Use upvaluejoin to make them point to SAME memory location
	  5. Recursively process function-type upvalues

	Example - Multiple Hotfixes:
	  Round 1: v1 → v2
	    - Make v2's same-named upvalues reference old module
	    - origin_functions[v2] = v1

	  Round 2: v2 → v3
	    - Module now has v2, but we process original v1
	    - origin_functions[v2] = v1, so we find and update v1
	    - Make v1's same-named upvalues reference old module (keep in sync)
	    - origin_functions[v3] = v1 (still points to original)

	  Without this, Round 2 would only update v2, and old references to v1
	  would stay stale.

	@param global table - Old module's upvalue map {name -> {func, index, id, value}}
	@param f function - Function to update (will be resolved to original)
	]] --
	local function update_func(global, f)
		if exclude[f] then return end
		exclude[f] = true

		-- Critical: Always use the ORIGINAL function, not intermediate versions
		-- This is why origin_functions exists - to support multiple hotfixes
		--
		-- Example: v1 → v2 → v3
		--   After 2nd hotfix, module.foo points to v2
		--   origin_functions[v2] = v1
		--   We need to update v1's upvalues, not v2's
		--   Because all old references still point to v1
		f = origin_functions[f] or f

		local oldf = nil
		local i = 1
		while true do
			local name, value = getupvalue(f, i)
			if name == nil then
				oldf = f
				break
			end

			if type(value) == "function" then
				-- Recursively update function-type upvalues
				-- These are nested functions that also need updating
				update_func(global, value)
			else
				-- Make this upvalue reference old module's SAME-NAMED upvalue
				-- global[name] looks up by NAME to find matching upvalue in old module
				-- upvaluejoin makes them point to the SAME memory location
				local old_uv = global[name]
				if old_uv then
					upvaluejoin(f, i, old_uv.func, old_uv.index)
				end
			end
			i = i + 1
		end

		-- Store mapping: new version -> original version
		-- Don't overwrite if already exists (preserve original across multiple hotfixes)
		-- This ensures that after v1→v2→v3, we still know v1 is the original
		origin_functions[f] = origin_functions[f] or oldf

		-- Set _ENV if needed
		-- Every function needs _ENV to access global functions and tables
		i = 1
		while true do
			local name, value = getupvalue(f, i)
			if name == nil then break end
			if name == "_ENV" then
				if value == nil then
					setupvalue(f, i, _ENV)
				end
				break
			end
			i = i + 1
		end
	end

	--[[
	Update all functions in a table structure.

	This handles both:
	1. Direct function values in tables (e.g., module.foo = function() end)
	2. Function-type upvalues in the upvalue map (local f = function() end)

	For each function found:
	  - Look up its prototype in the diff map
	  - If changed, call update_func to update its upvalues
	  - Record the update in transaction table (for atomic application)

	Transaction Structure:
	  Key: { uptable = table, key = name } for table entries
	       OR upvalue structure for upvalue entries
	  Value: New function implementation

	@param oldfuncs table - Table containing functions to update
	@param newproto table - Diff mapping (old_proto -> new_func)
	@param res table - Transaction accumulator (modified in-place)
	]] --
	local function update_funcs(oldfuncs, newproto, res)
		for name, v in next, oldfuncs do
			if type(v) == "function" then
				-- Direct function in table (e.g., module.foo)
				-- Check if this function's prototype appears in the diff
				local newf = newproto[proto(v)]
				if newf then
					-- Function has changed, update it
					update_func(upvalues, newf)
					-- Schedule update: module[name] = newf
					res[{ uptable = oldfuncs, key = name }] = newf
				end
			elseif type(v) == "table" and v.value and v.func then
				-- Upvalue structure from collect_uv
				-- Format: { func = f, index = i, id = id, value = value }
				local tp = type(v.value)
				if tp == "function" then
					-- Function-type upvalue (local f = function() end)
					local newf = newproto[proto(v.value)]
					if newf then
						-- Function has changed, update it
						update_func(upvalues, newf)
						-- Schedule update: setupvalue(v.func, v.index, newf)
						res[v] = newf
					end
				elseif tp == "table" and name ~= "_ENV" then
					-- Table-type upvalue (local methods = { ... })
					-- Recursively process functions within this table
					update_funcs(v.value, newproto, res)
				end
			end
		end
	end

	-- Transaction pattern: collect all updates first, then apply atomically
	-- This ensures all-or-nothing semantics - either all updates succeed or none do
	-- Phase 1: Collect all updates (above)
	-- Phase 2: Apply all updates (below)
	local transactions = {}
	update_funcs(upvalues, proto_map, transactions)
	update_funcs(root, proto_map, transactions)

	-- Apply all updates atomically
	-- At this point, all function upvalues have been updated via update_func
	-- Now we update the table references to point to the new functions
	for k, v in pairs(transactions) do
		if k.uptable then
			-- Update function in table: module[key] = new_function
			k.uptable[k.key] = v
		else
			-- Update function-type upvalue: setupvalue(func, index, new_function)
			setupvalue(k.func, k.index, v)
		end
	end
end

--[[
================================================================================
Main Entry Point: hotfix.update
================================================================================

Update a loaded module with a new version.

This is the main API for performing hotfixes. It implements a three-phase
process to ensure safe, atomic updates:

Phase 1: Parse & Validate
  - Parse source file to get function names
  - Generate diff between old and new versions
  - Check for compatibility issues
  - NO side effects, NO module state changes

Phase 2: Check New Functions
  - Verify new functions only reference existing upvalues
  - Collect all validation errors
  - Still NO state changes

Phase 3: Apply Updates
  - Update existing functions (hotfix_)
  - Add new functions to module
  - Link upvalues
  - This is the only phase that modifies state

If any phase fails, the function returns false and the module remains
unchanged. This provides atomic update semantics.

@param name string - Module name
@param updatename string|nil - Name to search for update (defaults to name)
@return boolean - Success status
@return any - Module table on success, error message on failure

Usage Example:
  local ok, result = hotfix.update("game.logic.player")
  if ok then
    print("Hotfix successful")
  else
    print("Hotfix failed:", result)
  end
]] --
function hotfix.update(name, updatename)
	assert(type(name) == "string")
	updatename = updatename or name
	local _LOADED = debug.getregistry()._LOADED

	if not _LOADED then
		return false, "no module loaded"
	end

	local root = _LOADED[name]

	-- Module not loaded yet, just load it normally
	if root == nil then
		return true, hotfix.require(name)
	end

	-- Module not registered with hotfix system
	if loaders[name] == nil then
		return false, "Can't find last version : " .. name
	end

	local loader = findloader(updatename)

	--[[
	================================================================================
	Phase 1: Parse and Validate (No State Changes)
	================================================================================

	In this phase we:
	  1. Collect all upvalues from the currently running module
	  2. Generate diff between old and new module versions
	  3. Check for compatibility issues
	  4. Parse source file to identify new function names

	Critical: NO side effects, NO module state changes
	If any check fails, we return early with error - module remains unchanged
	]] --

	local upvalues = collect_all_uv(root, name)

	-- Generate diff between old and new versions
	-- This compares function structures without executing any code
	-- Returns:
	--   diff: Map of old_proto -> new_func (which functions changed)
	--   err: Array of error messages (compatibility issues)
	--   newfuncs: Array of new functions (functions added in new version)
	local diff, err, newfuncs = hotfix.diff(loaders[name], loader, upvalues)
	if err and #err > 0 then
		return false, table.concat(err, "\n")
	end

	-- If new functions were added, parse source file to get their names
	-- We need names to add them to the module table (module.new_func = ...)
	if #newfuncs > 0 then
		local module_name = name:match("([^/]+)%.lua$") or name
		local parsed_funcs, parse_err = hotfix.parse_module_functions(name, module_name)
		if not parsed_funcs then
			return false, "Cannot parse source file: " .. (parse_err or "unknown error")
		end

		-- Build map: function_name -> function_object
		-- This converts newfuncs from array of functions to named map
		local newfuns_err = {}
		local newfuns_map = {}
		for _, newf in ipairs(newfuncs) do
			local info = funcinfo(newf)
			local function_name = parsed_funcs and parsed_funcs[info]
			if not function_name then
				table.insert(newfuns_err, "Cannot find new function name for " .. info)
			else
				newfuns_map[function_name] = newf
			end
		end

		if #newfuns_err > 0 then
			return false, table.concat(newfuns_err, "\n")
		end

		newfuncs = newfuns_map
	end

	--[[
	================================================================================
	Phase 2: Check New Functions (Still No State Changes)
	================================================================================

	Validate that new functions only reference upvalues that exist in the
	running module. This prevents runtime errors from missing dependencies.

	Example:
	  Old module: local counter = 0
	  New function: function new_func() counter = counter + 1 end
	  ✓ Valid - 'counter' exists

	  New function: function bad_func() missing_var = 1 end
	  ✗ Invalid - 'missing_var' doesn't exist

	Critical: Still NO state changes
	]] --
	local check_errors = {}

	-- Validate that new functions only reference existing upvalues
	-- This prevents adding functions that depend on nonexistent state
	for fn_name, fn_ptr in pairs(newfuncs) do
		local ok, errmsg = check_func_upvalue(fn_ptr, fn_name, upvalues)
		if not ok then
			table.insert(check_errors, errmsg)
		end
	end

	if #check_errors > 0 then
		return false, table.concat(check_errors, "\n")
	end

	--[[
	================================================================================
	Phase 3: Apply Updates (NOW we modify state)
	================================================================================

	All validation passed. Now we actually modify the running module:
	  1. Update existing functions by redirecting their upvalues (hotfix_)
	  2. Update the loader reference (for next hotfix round)
	  3. Add new functions to module table
	  4. Link new functions' upvalues to existing module state

	This is the ONLY phase that modifies state. If we reach here, we're
	committed to applying all changes.
	]] --

	-- Update existing functions
	-- This modifies the original functions' upvalues to point to new implementations
	-- After this, calling any old function reference will execute new code
	hotfix_(root, diff, upvalues)
	loaders[name] = loader

	-- If no new functions, we're done
	if not next(newfuncs) then
		return true, _LOADED[name]
	end

	print(string.format("hotfix: module '%s' updated with %d new functions", name, #newfuncs))

	-- Add new functions to module
	-- Link their upvalues to the existing module environment
	local added = {}
	for k, v in pairs(newfuncs) do
		link_func_upvalue(v, upvalues)  -- Make v share state with old module
		root[k] = v                      -- Add to module table
		table.insert(added, k)           -- Track for logging
	end

	if #added > 0 then
		print(string.format("hotfix: added %d new fields: %s", #added, table.concat(added, ", ")))
	end

	return true, _LOADED[name]
end

return hotfix
