local clonefunc = require "clonefunc"

local hardreload = {}

local function same_proto(f1,f2)
	local uv = {}
	local i = 1
	while true do
		local name = debug.getupvalue(f1, i)
		if name == nil then
			break
		end
		if name ~= "_ENV" then	-- ignore _ENV
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
		if name ~= "_ENV"  and uv[name] == nil then
			return false	-- f2 add a new upvalue
		end
		uv[name] = nil
		i = i + 1
	end
end

function hardreload.diff(m1, m2)
	local clone = clonefunc.clone
	local proto = clonefunc.proto

	local diff = {}
	local err = nil

	local function funcinfo(f)
		local info = debug.getinfo(f, "S")
		return string.format("%s(%d-%d)",info.short_src,info.linedefined,info.lastlinedefined)
	end

	local function diff_(a,b)
		local p1,n1 = proto(a)
		local p2,n2 = proto(b)
		if p1 == nil or p2 == nil or n1 ~= n2 then
			err = err or {}
			table.insert(err, funcinfo(a) .. "/" .. funcinfo(b))
			return
		end
		if not same_proto(a,b) then
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

function hardreload.addsearcher(fn)
	searchers[#searchers+1] = fn
end

local function findloader(name)
	local msg = {}
	for _, loader in ipairs(searchers) do
		local f , extra = loader(name)
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
local old_functions = setmetatable({}, {__mode = "k"})

function hardreload.require(name)
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

function hardreload.register(name, loader, ret)
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

local function hotfix(root, diff)

	local proto = clonefunc.proto

	local getupvalue = debug.getupvalue
	local upvalueid = debug.upvalueid
	local upvaluejoin = debug.upvaluejoin
	local setupvalue = debug.setupvalue

	local exclude = {}

	local function collect_uv(f , uv, proto_map)
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
					collect_uv(value, uv, proto_map)
				end
			end

			i = i + 1
		end
	end

	local function collect_all_uv(funcs, proto_map)
		local global = {}
		for _, v in pairs(funcs) do
			if type(v) == "function"  then
				collect_uv(v, global, proto_map)
			end
		end
		if not global["_ENV"] then
			global["_ENV"] = {func = collect_uv, index = 1}
		end
		return global
	end

	local function update_func(global, f)
		if exclude[f] then
			--print("exclude", f)
			return
		end
		exclude[f] = true

		local oldf = nil
		f = old_functions[f] or f	-- find origin version
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
				upvaluejoin(f, i, old_uv.func, old_uv.index)
			end
			i = i + 1
		end
		old_functions[f] = old_functions[f] or oldf	-- don't clear old_functions

		i = 1
		while true do
			local name , value = getupvalue(f, i)
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

	local global = collect_all_uv(root, diff)

	local new_up_functions = {}
	for name, v in pairs(global) do
		if type(v.value) == "function" then
			local newf = diff[proto(v.value)]
			if newf then
				--print("hotfix", name)
				update_func(global, newf)
				new_up_functions[v] = newf
			end
		end
	end

	local new_functions = {}
	for name, v in pairs(root) do
		if type(v) == "function" then
			local newf = diff[proto(v)]
			-- assert(newf, name)
			if newf then
				--print("hotfix", name)
				update_func(global, newf)
				new_functions[name] = newf
			end
		end
	end

	for k, v in pairs(new_up_functions) do
		setupvalue(k.func,  k.index, v)
	end

	for name, v in pairs(new_functions) do
		root[name] = v
	end
end

---1. Cannot hotfix metatable functions
---2. Cannot hotfix functions that are assigned values to other variables
function hardreload.reload_simple(name, updatename)
	assert(type(name) == "string")
	updatename = updatename or name
	local _LOADED = debug.getregistry()._LOADED
	if _LOADED[name] == nil then
		return true, hardreload.require(name)
	end
	if loaders[name] == nil then
		return false, "Can't find last version : " .. name
	end

	local loader = findloader(updatename)
	local diff, err = hardreload.diff(loaders[name], loader)
	if err then
		-- failed
		if loaders[name] == origin[name] then
			-- first time reload
			return false, table.concat(err, "\n")
		end
		local _, err = hardreload.diff(origin[name], loader)
		if err then
			-- add upvalue not exist in origin version
			return false, table.concat(err, "\n")
		end
	end

	hotfix(_LOADED[name], diff)
	loaders[name] = loader
	return true, _LOADED[name]
end

return hardreload
