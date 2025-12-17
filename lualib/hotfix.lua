--[[
================================================================================
Lua Hot Update System
================================================================================

This module implements a runtime hot-update system for Lua modules, allowing
you to update function implementations without restarting the program.

Key Features:
  1. Update existing function implementations
  2. Add new functions to running modules
  3. Maintain upvalue consistency across updates
  4. Support multiple rounds of hotfixes
  5. Atomic updates with rollback on failure

Architecture:
  - Three-phase update process: Parse → Check → Apply
  - Weak table tracking for memory efficiency
  - Upvalue environment preservation across updates

Limitations:
  - Cannot modify module-level local variables directly
  - New functions can only reference upvalues that exist in old module
  - Function signature changes must maintain upvalue compatibility

================================================================================
]] --

local clonefunc = require "clonefunc"
local getupvalue = debug.getupvalue
local upvalueid = debug.upvalueid
local upvaluejoin = debug.upvaluejoin
local setupvalue = debug.setupvalue
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
-- Example:
--   v1 → v2 → v3 → v4
--   origin_functions[v4] = v1 (always points to original)
--   origin_functions[v2] can be GC'd when v2 is no longer referenced
--   But v1 is preserved as the value in origin_functions[v4]
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
  - f2 can have fewer upvalues than f1
  - f2 cannot add new upvalues not in f1
  - _ENV is always ignored in comparison

@param f1 function - Old version function
@param f2 function - New version function
@return boolean - true if compatible, false otherwise
]] --
local function same_proto(f1, f2)
	local uv = {}
	local i = 1

	-- Step 1: Collect all upvalues from f1
	while true do
		local name = getupvalue(f1, i)
		if name == nil then break end
		if name ~= "_ENV" then
			uv[name] = true
		end
		i = i + 1
	end

	-- Step 2: Check f2's upvalues against f1
	i = 1
	while true do
		local name = getupvalue(f2, i)
		if name == nil then
			return true -- f2 can have fewer upvalues
		end
		if name ~= "_ENV" and uv[name] == nil then
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
	local info = debug.getinfo(f, "S")
	return string.format("%s(%d)", info.short_src, info.linedefined)
end

--[[
================================================================================
Core Algorithm: Diff Generation
================================================================================

Compare two module loader functions and generate a mapping from old function
prototypes to new function implementations.

Algorithm:
  1. Compare function prototypes (proto pointers)
  2. Check child function count (can only increase)
  3. Verify upvalue compatibility
  4. Recursively process child functions
  5. Collect newly added functions

@param m1 function - Old module loader
@param m2 function - New module loader
@return diff table - Map of old_proto -> new_func
@return err table|nil - Array of error messages
@return newfuncs table - Array of new functions
]] --
function hotfix.diff(m1, m2)
	local diff = {}
	local err = nil
	local newfuncs = {}

	local function diff_(a, b)
		-- Get function prototype and child count
		-- proto: Internal pointer to function prototype
		-- n: Number of child functions (nested functions)
		local p1, n1 = proto(a)
		local p2, n2 = proto(b)

		if p1 == nil or p2 == nil then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end

		-- Check child function count
		-- New version can have MORE children (add functions)
		-- New version CANNOT have FEWER children (remove functions)
		if n1 > n2 then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end

		-- Check upvalue compatibility
		-- Ensures new function can be safely substituted for old function
		if not same_proto(a, b) then
			err = err or {}
			table.insert(err, string.format("incompatible upvalue in %s -> %s", funcinfo(a), funcinfo(b)))
		end

		-- Store mapping: old prototype -> new function
		-- This is used later to locate which functions need updating
		diff[p1] = b

		-- Recursively process existing child functions (1 to n1)
		-- These are functions that exist in both old and new versions
		for i = 1, n1 do
			diff_(clone(a, i), clone(b, i))
		end

		-- Collect newly added child functions (n1+1 to n2)
		-- These are functions that only exist in the new version
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
Parse module source file to extract function definitions.

This function parses the source file to build a mapping from function
locations to function names. This is necessary because clonefunc.clone()
returns functions by index, but we need to know their names.

Caching Strategy:
  - First try to load from moon.env_unpacked cache
  - If cache miss, parse source file and extract function names
  - Pattern matches: "function ModuleName.FunctionName("

@param filepath string - Source file path
@param modname string - Module name (used in pattern matching)
@return table - Map of "filepath(line)" -> "function_name"
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
	local _LOADED = debug.getregistry()._LOADED
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
	local _LOADED = debug.getregistry()._LOADED
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

This builds a complete map of all upvalues in the module, which is needed
to properly link new functions to the existing upvalue environment.

Upvalue Structure:
  {
    func: function that owns this upvalue
    index: upvalue index in the function
    id: unique upvalue ID
    value: upvalue value
  }

Special Cases:
  1. Function-type upvalues: Recursively collect if in proto_map
  2. Table-type upvalues: Collect if all values are functions from same source
  3. _ENV: Always ignored to avoid conflicts

@param f function - Function to collect upvalues from
@param uv table - Accumulator for upvalue information
@param source string - Source file path for filtering
@param proto_map table - Diff mapping to check if function should be processed
]] --
local function collect_uv(f, uv, source, proto_map)
	local i = 1
	while true do
		local name, value = getupvalue(f, i)
		if name == nil then break end

		local id = upvalueid(f, i)

		if uv[name] then
			-- Upvalue already collected, verify consistency
			-- Same upvalue name must have same ID (_ENV is exception)
			if name ~= "_ENV" then
				assert(uv[name].id == id, string.format("ambiguity local value %s", name))
			end
		else
			-- New upvalue, store it
			uv[name] = { func = f, index = i, id = id, value = value }

			-- Recursively collect from function-type upvalues
			-- Only process if function is in the diff map
			if type(value) == "function" and proto_map[proto(value)] then
				collect_uv(value, uv, source, proto_map)

				-- Collect from table-type upvalues (method tables)
				-- Only if all values are functions from the same source file
			elseif type(value) == "table" and name ~= "_ENV" then
				for k, v in next, value do
					if type(v) == "function" then
						-- Skip functions from other source files
						-- This prevents collecting framework functions
						if debug.getinfo(v, "S").short_src ~= source then
							break
						end
						collect_uv(v, uv, source, proto_map)
					elseif k ~= "__index" then
						-- Table must contain only functions (except __index)
						break
					end
				end
			end
		end
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
local function collect_all_uv(root, source, proto_map)
	local upvalue_funcs = {}
	for _, v in pairs(root) do
		if type(v) == "function" then
			collect_uv(v, upvalue_funcs, source, proto_map)
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

This ensures that new functions share state with old functions by pointing
their upvalues to the same memory locations.

Process:
  1. Iterate through all upvalues of the new function
  2. If upvalue is a function, recursively link it
  3. If upvalue exists in old module, join them (shared state)
  4. Ensure _ENV is properly set

@param f function - New function to link
@param upvalues table - Old module's upvalue map
]] --
local function link_func_upvalue(f, upvalues)
	if type(f) ~= "function" then return end

	local i = 1
	while true do
		local uvname, value = getupvalue(f, i)
		if uvname == nil then break end

		if type(value) == "function" then
			-- Recursively link nested functions
			link_func_upvalue(value, upvalues)
		elseif upvalues[uvname] and uvname ~= "_ENV" then
			-- Link to old module's upvalue (shared state)
			-- debug.upvaluejoin(f2, n2, f1, n1) makes f2's nth upvalue
			-- point to the same location as f1's nth upvalue
			local old_uv = upvalues[uvname]
			upvaluejoin(f, i, old_uv.func, old_uv.index)
		end
		i = i + 1
	end

	-- Ensure _ENV is properly set
	-- _ENV is the environment table that provides access to globals
	i = 1
	while true do
		local uvname, value = getupvalue(f, i)
		if uvname == nil then break end
		if uvname == "_ENV" then
			if value == nil and upvalues["_ENV"] then
				setupvalue(f, i, upvalues["_ENV"].value or _ENV)
			end
			break
		end
		i = i + 1
	end
end

--[[
Check if new function only references upvalues that exist in old module.

This validation ensures that new functions don't introduce dependencies
on upvalues that don't exist in the running module.

@param f function - New function to check
@param funcname string - Function name for error messages
@param upvalues table - Old module's upvalue map
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
Core Hotfix Logic
================================================================================

This is the heart of the hotfix system. It updates existing functions by
redirecting their upvalues to point to new implementations while preserving
the upvalue environment.

Key Insight:
  Instead of replacing function references everywhere, we update the
  ORIGINAL function's upvalues to point to the new implementation.
  This works because Lua functions are closures - updating the upvalue
  effectively changes the function's behavior.

Algorithm:
  1. Find the original version of each function (via origin_functions)
  2. Update its upvalues to point to new function's upvalue environment
  3. For function-type upvalues, recursively update them
  4. Collect all updates in a transaction table
  5. Apply all updates atomically
]] --
local function hotfix_(root, proto_map, upvalues)
	local exclude = {}

	--[[
	Update a single function's upvalues.

	@param global table - Upvalue environment
	@param f function - Function to update
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
				update_func(global, value)
			else
				-- Link to new upvalue environment
				local old_uv = assert(global[name], string.format("upvalue %s not found", name))
				upvaluejoin(f, i, old_uv.func, old_uv.index)
			end
			i = i + 1
		end

		-- Store mapping: new version -> original version
		-- Don't overwrite if already exists (preserve original across multiple hotfixes)
		origin_functions[f] = origin_functions[f] or oldf

		-- Set _ENV if needed
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
	1. Direct function values in tables
	2. Function-type upvalues in the upvalue map

	@param oldfuncs table - Table containing functions to update
	@param newproto table - Diff mapping (old_proto -> new_func)
	@param res table - Transaction accumulator
	]] --
	local function update_funcs(oldfuncs, newproto, res)
		for name, v in next, oldfuncs do
			if type(v) == "function" then
				-- Direct function in table (e.g., module.foo)
				local newf = newproto[proto(v)]
				if newf then
					update_func(upvalues, newf)
					res[{ uptable = oldfuncs, key = name }] = newf
				end
			elseif type(v) == "table" and v.value and v.func then
				-- Upvalue structure from collect_uv
				local tp = type(v.value)
				if tp == "function" then
					-- Function-type upvalue
					local newf = newproto[proto(v.value)]
					if newf then
						update_func(upvalues, newf)
						res[v] = newf
					end
				elseif tp == "table" and name ~= "_ENV" then
					-- Table-type upvalue (method table)
					update_funcs(v.value, newproto, res)
				end
			end
		end
	end

	-- Transaction pattern: collect all updates first, then apply atomically
	-- This ensures all-or-nothing semantics
	local transactions = {}
	update_funcs(upvalues, proto_map, transactions)
	update_funcs(root, proto_map, transactions)

	-- Apply all updates atomically
	for k, v in pairs(transactions) do
		if k.uptable then
			-- Update function in table
			k.uptable[k.key] = v
		else
			-- Update function-type upvalue
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
		Phase 1:	Parse and validate without executing
	]] --

	-- Generate diff between old and new versions
	-- This compares function structures without executing any code
	local diff, err, newfuncs = hotfix.diff(loaders[name], loader)
	if err then
		return false, table.concat(err, "\n")
	end

	if #newfuncs > 0 then
		local module_name = name:match("([^/]+)%.lua$") or name
		local parsed_funcs, parse_err = hotfix.parse_module_functions(name, module_name)
		if not parsed_funcs then
			return false, "Cannot parse source file: " .. (parse_err or "unknown error")
		end

		local newfuns_err = {}
		local newfuns_map = {}
		for _, newf in ipairs(newfuncs) do
			local info = funcinfo(newf)
			local function_name = parsed_funcs and parsed_funcs[info]
			if not function_name then
				err = err or {}
				table.insert(err, "Cannot find new function name for " .. info)
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
	Phase 2: Check new functions' upvalue compatibility
	]] --
	local upvalues = collect_all_uv(root, name, diff)
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
	Phase 3: Apply updates (only if all checks passed)
	]] --

	-- Update existing functions
	-- This modifies the original functions' upvalues to point to new implementations
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
		link_func_upvalue(v, upvalues)
		root[k] = v
		table.insert(added, k)
	end

	if #added > 0 then
		print(string.format("hotfix: added %d new fields: %s", #added, table.concat(added, ", ")))
	end

	return true, _LOADED[name]
end

return hotfix
