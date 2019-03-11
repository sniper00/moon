--ref https://github.com/openresty/lua-resty-redis
--not support transactions and pub/sub
local sub = string.sub
local byte = string.byte
local type = type
local pairs = pairs
local setmetatable = setmetatable
local tonumber = tonumber
local tostring = tostring

local seri	= require("seri")
local moon	= require("moon")
local socket = require("moon.net.socket")

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

local sock

function redis.new()
	if not sock then
		sock = socket.new()
	end
	local t = {}
	t.sock = sock
	if not t.sock then
		return nil, "socket initialized failed"
	end
	return setmetatable(t, mt)
end

function redis:connect(ip, port, timeout)
	if not self.sock then
		return nil, "socket not initialized"
	end

	if self.conn then
		return true
	end

	self.state = "connecting"
	if timeout then
		self.sock:settimeout(timeout)
	end
	self.conn = self.sock:connect(ip, port)
	return(self.conn ~= nil)
end

function redis:async_connect(ip, port, timeout)
	if self.conn then
		return true
	end
	if timeout then
		self.sock:settimeout(timeout)
	end
	self.conn = self.sock:co_connect(ip, port)
	return(self.conn ~= nil)
end

function redis:close()
	if not self.conn then
		return nil, "closed"
	end
	print("force close redis")
	self.conn:close()
end

function redis:_readreplay()
	local line, err1 = self.conn:co_read('\r\n')
	if not line then
		return nil, err1
	end

	local prefix = byte(line)

	if prefix == 36 then -- char '$'
		local size = tonumber(sub(line, 2))
		if size < 0 then
			return moon.null
		end

		local data, err2 = self.conn:co_read(size)
		if not data then
			return nil, err2
		end

		local dummy, err3 = self.conn:co_read(2)
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

	if not self.conn then
		return nil, "closed"
	end

	local req = _gen_req(args)

	if self.reqs then
		self.reqs[#self.reqs + 1] = req
		return
	end

	-- print("request: ", table.concat(req))
	if not self.conn:send(seri.concat(req)) then
		return nil, "closed"
	end

	local res,err = self:_readreplay()
	if res == nil then
		self.conn = nil
	end
	return res,err
end

function redis:readreplay()
	if not self.conn then
		return nil, "closed"
	end

	local res, err = self:_readreplay()
	if res == nil then
		self.conn = nil
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

	local conn = rawget(self, "conn")
	if not conn then
		return nil, "not initialized"
	end

	local ok = conn:send(seri.concat(reqs))
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
			self.conn = nil
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
