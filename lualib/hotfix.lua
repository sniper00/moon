local clonefunc = require "clonefunc"

local hotfix = {}

local function same_proto(f1, f2)
	local uv = {}
	local i = 1
	while true do
		local name = debug.getupvalue(f1, i)
		if name == nil then
			break
		end
		if name ~= "_ENV" then -- ignore _ENV
			uv[name] = true
		end
		i = i + 1
	end
	i = 1
	while true do
		local name = debug.getupvalue(f2, i)
		if name == nil then
			-- new version can has less upvalue (uv is not empty)
			return true
		end
		if name ~= "_ENV" and uv[name] == nil then
			return false -- f2 add a new upvalue
		end
		uv[name] = nil
		i = i + 1
	end
end

---Recursive detection function m1's proto and function m2's proto have the same count
---@return table, table? @ function's proto <-> new function
function hotfix.diff(m1, m2)
	local clone = clonefunc.clone
	local proto = clonefunc.proto

	local diff = {}
	local err = nil

	local function funcinfo(f)
		local info = debug.getinfo(f, "S")
		return string.format("%s(%d-%d)", info.short_src, info.linedefined, info.lastlinedefined)
	end

	local function diff_(a, b)
		local p1, n1 = proto(a)
		local p2, n2 = proto(b)
		if p1 == nil or p2 == nil or n1 ~= n2 then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end
		if not same_proto(a, b) then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
		end
		diff[p1] = b
		for i = 1, n1 do
			diff_(clone(a, i), clone(b, i))
		end
	end

	diff_(m1, m2)
	return diff, err
end

local searchers = {}

function hotfix.addsearcher(fn)
	searchers[#searchers + 1] = fn
end

---@return function, string?
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

local loaders = {}
local origin = {}
local old_functions = setmetatable({}, { __mode = "k" })

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

local function hotfix_(root, proto_map, short_source)
	local proto = clonefunc.proto

	local getupvalue = debug.getupvalue
	local upvalueid = debug.upvalueid
	local upvaluejoin = debug.upvaluejoin
	local setupvalue = debug.setupvalue

	local exclude = {}

	local function collect_uv(f, uv, source)
		local i = 1
		while true do
			local name, value = getupvalue(f, i)
			if name == nil then
				break
			end
			local id = upvalueid(f, i)

			if uv[name] then
				assert(uv[name].id == id, string.format("ambiguity local value %s", name))
			else
				uv[name] = { func = f, index = i, id = id, value = value }
				if type(value) == "function" and proto_map[proto(value)] then
					collect_uv(value, uv, source)
				elseif type(value) == "table" and name ~= "_ENV" then
					for k, v in next, value do
						if type(v) == "function" then
							if debug.getinfo(v, "S").short_src ~= source then
								--print("break reason source does not match", name, _, v, source, debug.getinfo(v, "S").short_src)
								---skip functions not in source
								break
							end
							collect_uv(v, uv, source)
						elseif k ~= "__index" then
							--print("break reason all value must be function type", name, value, v)
							---all value must be function type
							break
						end
					end
				end
			end
			i = i + 1
		end
	end

	---collect old verion function's upvalue
	local function collect_all_uv(funcs, source)
		local global = {}
		for _, v in pairs(funcs) do
			if type(v) == "function" then
				collect_uv(v, global, source)
			end
		end
		if not global["_ENV"] then
			global["_ENV"] = { func = collect_uv, index = 1 }
		end
		return global
	end

	local function update_func(global, f)
		if exclude[f] then
			return
		end
		exclude[f] = true
		f = old_functions[f] or f -- find origin version

		local oldf = nil
		local i = 1
		while true do
			local name, value = getupvalue(f, i)
			if name == nil then
				oldf = f
				break
			end
			if type(value) == "function" then
				update_func(global, value)
			else
				local old_uv = assert(global[name])
				---let new function refer old function's upvalue
				upvaluejoin(f, i, old_uv.func, old_uv.index)
			end
			i = i + 1
		end
		old_functions[f] = old_functions[f] or oldf -- don't clear old_functions

		i = 1
		while true do
			local name, value = getupvalue(f, i)
			if name == nil then
				break
			end
			if name == "_ENV" then
				if value == nil then
					setupvalue(f, i, _ENV)
				end
				break
			end
			i = i + 1
		end
	end

	local upvalues = collect_all_uv(root, short_source)
	-- local env = upvalues['_ENV']
	-- upvalues['_ENV'] = nil
	-- print_r(upvalues)
	-- upvalues['_ENV'] = env

	local function update_funcs(oldfuncs, newproto, res)
		for name, v in next, oldfuncs do
			if type(v) == "function" then
				local newf = newproto[proto(v)]
				if newf then
					--print("2. hotfix", name)
					update_func(upvalues, newf)
					res[{ uptable = oldfuncs, key = name }] = newf
				else
					--print("hotfix skip: hotfix not found proto", name, v)
					break
				end
			elseif type(v) == "table" and v.value and v.func then
				local tp = type(v.value)
				if tp == "function" then
					local newf = newproto[proto(v.value)]
					if newf then
						--print("1. hotfix", name)
						update_func(upvalues, newf)
						res[v] = newf
					else
						--print("hotfix skip: hotfix not found proto", name, v)
					end
				elseif tp == "table" and name ~= "_ENV" then
					--print("3. step in:", name)
					update_funcs(v.value, newproto, res)
				end
			end
		end
	end

	local transactions = {}

	update_funcs(upvalues, proto_map, transactions)
	update_funcs(root, proto_map, transactions)

	for k, v in pairs(transactions) do
		if k.uptable then
			---set new function to origin parent
			k.uptable[k.key] = v
		else
			---set new function's function-type upvalue
			setupvalue(k.func, k.index, v)
		end
	end
end

---only hotfix functions defined in current source file
function hotfix.update(name, updatename)
	assert(type(name) == "string")
	updatename = updatename or name
	local _LOADED = debug.getregistry()._LOADED
	if _LOADED[name] == nil then
		return true, hotfix.require(name)
	end
	if loaders[name] == nil then
		return false, "Can't find last version : " .. name
	end

	local loader = findloader(updatename)
	local diff, err = hotfix.diff(loaders[name], loader)
	if err then
		-- failed
		if loaders[name] == origin[name] then
			-- first time reload
			return false, table.concat(err, "\n")
		end
		local _, err = hotfix.diff(origin[name], loader)
		if err then
			-- add upvalue not exist in origin version
			return false, table.concat(err, "\n")
		end
	end

	hotfix_(_LOADED[name], diff, name)
	loaders[name] = loader
	return true, _LOADED[name]
end

return hotfix
