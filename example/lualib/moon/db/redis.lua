--ref https://github.com/openresty/lua-resty-redis
--not support transactions and pub/sub
local sub = string.sub
local byte = string.byte
local type = type
local pairs = pairs
local setmetatable = setmetatable
local tonumber = tonumber
local tostring = tostring
local select = select
local rawget = rawget
local seri	= require("seri")
local moon	= require("moon")
local socket = require("moon.socket")

local _read = socket.read
local _readline = socket.readline
local _write = socket.write

local new_table = table.new or function() return {} end

local common_cmds = {
	"get",
	"set",
	"mget",
	"mset",
	"del",
	"incr",
	"decr", -- Strings
	"llen",
	"lindex",
	"lpop",
	"lpush",
	"lrange",
	"linsert", -- Lists
	"hexists",
	"hget",
	"hgetall",
	"hset",
	"hmget",
	--[[ "hmset", ]]
	"hdel", -- Hashes
	"smembers",
	"sismember",
	"sadd",
	"srem",
	"sdiff",
	"sinter",
	"sunion", -- Sets
	"zrange",
	"zrangebyscore",
	"zrank",
	"zadd",
	"zrem",
	"zincrby", -- Sorted Sets
	"auth",
	"eval",
	"expire",
	"script",
	"sort" -- Others
}

local redis = new_table(0, 54)

redis.VERSION = '0.1'

local mt = {__index = redis}

function redis.new()
	local t = {}
	return setmetatable(t, mt)
end

function redis:connect(ip, port, timeout)
	if self.fd then
		return true
	end
	self.state = "connecting"
	self.fd = socket.connect(ip, port, moon.PTYPE_TEXT)
	if timeout then
		socket.settimeout(self.fd, timeout)
	end
	return self.fd
end

function redis:sync_connect(ip, port, timeout)
	if self.fd then
		return true
	end
	self.fd = socket.sync_connect(ip, port, moon.PTYPE_TEXT)
	if timeout then
		socket.settimeout(self.fd, timeout)
	end
	return assert(self.fd)
end

function redis:close()
	if not self.fd then
		return nil, "closed"
	end
	print("force close redis")
	socket.close(self.fd)
	self.fd:close()
end

function redis:_readreplay()
	local line, err1 = _readline(self.fd,'\r\n')
	if not line then
		return nil, err1
	end

	local prefix = byte(line)

	if prefix == 36 then -- char '$'
		local size = tonumber(sub(line, 2))
		if size < 0 then
			return moon.null
		end

		local data, err2 = _read(self.fd,size)
		if not data then
			return nil, err2
		end

		local dummy, err3 = _read(self.fd, 2)
		-- ignore CRLF
		if not dummy then
			return nil, err3
		end

		return data
	elseif prefix == 43 then -- char '+'
		-- print("status reply")
		return sub(line, 2)
	elseif prefix == 42 then -- char '*'
		local n = tonumber(sub(line, 2))
		-- print("multi-bulk reply: ", n)
		if n < 0 then
			return moon.null
		end

		local vals = new_table(n, 0)
		local nvals = 0
		for _ = 1, n do
			local res, err = self:_readreplay()
			if res then
				nvals = nvals + 1
				vals[nvals] = res
			elseif res == nil then
				return nil, err
			else
				-- be a valid redis error value
				nvals = nvals + 1
				vals[nvals] = {false, err}
			end
		end
		return vals
	elseif prefix == 58 then -- char ':'
		--print("integer reply",sub(line, 2))
		return tonumber(sub(line, 2))
	elseif prefix == 45 then -- char '-'
		-- print("error reply: ", n)
		return false, sub(line, 2)
	else
		-- when `line` is an empty string, `prefix` will be equal to nil.
		return nil, 'unknown prefix: \"' .. tostring(prefix) .. '\"'
	end
end

local function _gen_req(args)
	local nargs = #args

	local req = new_table(nargs * 5 + 1, 0)
	req[1] = "*" .. nargs .. "\r\n"
	local nbits = 2

	for i = 1, nargs do
		local arg = args[i]
		if type(arg) ~= "string" then
			arg = tostring(arg)
		end

		req[nbits] = "$"
		req[nbits + 1] = #arg
		req[nbits + 2] = "\r\n"
		req[nbits + 3] = arg
		req[nbits + 4] = "\r\n"

		nbits = nbits + 5
	end

	-- it is much faster to do string concatenation on the C land
	-- in real world (large number of strings in the Lua VM)
	return req
end

function redis:docmd(...)
	local args = {...}

	if not self.fd then
		return nil, "closed"
	end

	local req = _gen_req(args)

	if self.reqs then
		self.reqs[#self.reqs + 1] = req
		return
	end

	-- print("request: ", table.concat(req))
	if not _write(self.fd, seri.concat(req)) then
		return nil, "closed"
	end

	local res,err = self:_readreplay()
	if res == nil then
		self.fd = nil
	end
	return res,err
end

function redis:readreplay()
	if not self.fd then
		return nil, "closed"
	end

	local res, err = self:_readreplay()
	if res == nil then
		self.fd = nil
	end
	return res, err
end

for i = 1, #common_cmds do
	local cmd = common_cmds[i]
	redis[cmd] = function(self, ...)
		return redis.docmd(self, cmd, ...)
	end
end

function redis:hmset(hashname, ...)
	if select("#", ...) == 1 then
		local t = select(1, ...)

		local n = 0
		for _, _ in pairs(t) do
			n = n + 2
		end

		local array = new_table(n, 0)

		local i = 0
		for k, v in pairs(t) do
			array[i + 1] = k
			array[i + 2] = v
			i = i + 2
		end
		-- print("key", hashname)
		return self:docmd("hmset", hashname, table.unpack(array))
	end

	-- backwards compatibility
	return self:docmd("hmset", hashname, ...)
end

function redis:init_pipeline(n)
	self.reqs = new_table(n or 4, 0)
end


function redis:cancel_pipeline()
	self.reqs = nil
end

function redis:commit_pipeline()
	local reqs = rawget(self, "reqs")
	if not reqs then
		return nil, "no pipeline"
	end

	self.reqs = nil

	local fd = rawget(self, "fd")
	if not fd then
		return nil, "socket not initialized"
	end

	local ok = _write(self.fd, seri.concat(reqs))
	if not ok then
		return nil
	end

	local nvals = 0
	local nreqs = #reqs
	local vals = new_table(nreqs, 0)
	for _ = 1, nreqs do
		local res, err = self:_readreplay(conn)
		if res then
			nvals = nvals + 1
			vals[nvals] = res
		elseif res == nil then
			self.fd = nil
			return nil, err
		else
			-- be a valid redis error value
			nvals = nvals + 1
			vals[nvals] = {false, err}
		end
	end

	return vals
end

return redis
