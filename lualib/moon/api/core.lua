error("DO NOT REQUIRE THIS FILE")

--- lightuserdata buffer*
---@class buffer_ptr

--- lightuserdata message*
---@class message_ptr

--- lightuserdata char*
---@class cstring_ptr

---@class core
---@field public id integer @service's id
---@field public name string @service's name
---@field public timezone integer @server's local timezone
local core = {}

---same as os.clock()
---@return number @in seconds
function core.clock() end

---string's md5.
---@param data string
---@return string
function core.md5(data) end

---convert c-string(char* and size) to lua-string
---@param sz cstring_ptr
---@param len integer
---@return string
function core.tostring(sz, len) end

---@alias server_stats_options
---|>'"service.count"'      # return total services count
---| '"log.error"'         # return log error count

---@param opt? server_stats_options @ nil return all server stats
---@return string @get server stats in json format
function core.server_stats(opt) end

---if linked mimalloc, call mimalloc's collect
---@param force boolean @
function core.collect(force) end

-- ---create a timer
-- ---@param interval integer @ms
-- ---@return integer @timer id
-- function core.timeout(interval)
--     ignore_param(interval)
-- end

--- print console log
function core.log(loglv,...) end

--- set or get log level
---@param lv? string @DEBUG, INFO, WARN, ERROR
---@return integer @ log level
function core.loglevel(lv) end

--- get this service's cpu cost time
---@return integer
function core.cpu() end

---make a prefab message
---@param data string|buffer_ptr
---@return integer @prefab id
function core.make_prefab(data) end

---send prefab message
---@param receiver integer
---@param prefabid integer
---@param header string
---@param sessionid integer
---@param type integer
function core.send_prefab(receiver, prefabid, header, sessionid, type) end

-- --- send message from `sender` to `receiver`
-- ---@param receiver integer
-- ---@param data string|userdata @ message
-- ---@param header string
-- ---@param sessionid integer
-- function core.send(receiver, data, header, sessionid)
--     ignore_param(receiver, data, header, sessionid)
-- end

--- remove a service
function core.kill(addr) end

--- query **unique** service's address by name
function core.queryservice(name) end

--- set or get env
---@param key string
---@param value? string
---@return string
function core.env(key, value) end

---get worker thread info
---@return string
function core.wsate(workerid) end

--- let server exit: exitcode>=0 will wait all services quit.
---@param exitcode integer
function core.exit(exitcode) end

--- get total services's count()
---@return integer
function core.size() end

--- adjusts server time(millsecond)
---@param milliseconds integer
function core.adjtime(milliseconds) end

-- --- set lua callback
-- ---@param fn fun(msg:userdata,ptype:integer)
-- function core.callback(fn)
--     ignore_param(fn)
-- end

--- Get server timestamp in milliseconds
--- @return integer
function core.now() end

--- get message's field
---
--- - 'S' message:sender()
--- - 'R' message:receiver()
--- - 'E' message:sessionid()
--- - 'H' message:header()
--- - 'Z' message:bytes()
--- - 'N' message:size()
--- - 'B' message:buffer()
--- - 'C' message:buffer():data() and message:buffer():size()
---@param msg message_ptr
---@param pattern string
---@return ...
---@nodiscard
function core.decode(msg, pattern) end

---clone message, but share buffer field
---@param msg message_ptr
---@return userdata
function core.clone(msg) end

---release clone message
---@param msg message_ptr
function core.release(msg) end

---redirect a message to other service
function core.redirect(msg, header, receiver, mtype, sender, sessionid) end

---get error log count
---@return integer
function core.error_count() end

---@class asio
local asio = {}

---@param host string
---@param port integer
---@return boolean
function asio.try_open(host, port) end

---param protocol moon.PTYPE_SOCKET_TCP, moon.PTYPE_SOCKET_MOON, moon.PTYPE_SOCKET_WS
---@param host string
---@param port integer
---@param protocol integer
---@return integer
function asio.listen(host, port, protocol) end

---send data to fd
---@param fd integer
---@param data string|buffer_ptr
---@param flag? integer
---@return boolean
function asio.write(fd, data, flag) end

---@param fd integer
---@param m message_ptr
---@return boolean
function asio.write_message(fd, m) end

--- 设置读操作超时, 默认是0, 永远不会超时。为了处理大量链接的检测,实际超时检测并不严格，误差范围为[t, t+10)
---@param fd integer
---@param t integer @ seconds
---@return boolean
function asio.settimeout(fd, t) end

---@param fd integer
---@return boolean
function asio.setnodelay(fd) end

---@alias chunkmode
---| 'r' # read.
---| 'w' # write.
---| 'rw'
---| 'wr'

--- 对于PTYPE_SOCKET_MOON类型的协议, 默认最大长度是32767字节。
--- 可以设置chunkmode, 允许收发大于这个长度消息, 底层的处理是对消息进行切片。
---@param fd integer
---@param mode chunkmode
---@return boolean
function asio.set_enable_chunked(fd, mode) end

--- set send queue limit
---@param fd integer @ fd
---@param warnsize integer @ if send queue size > warnsize, print warnning log
---@param errorsize integer @ if send queue size > errorsize, print error log and close socket
function asio.set_send_queue_limit(fd, warnsize, errorsize) end

---@param fd integer
function asio.close(fd) end

---@param fd integer
---@return string @ format ip:port
function asio.getaddress(fd) end

---@param fd integer
---@param addr string @ addr bytes string. see udp callback function second param or user make_endpoint
---@param data string|userdata
---@return boolean
function asio.sendto(fd, addr, data) end

---@param fd integer
---@param host string
---@param port integer
---@return boolean
function asio.udp_connect(fd, host, port) end

---@param host string
---@param port integer
---@return string @addr bytes string
function asio.make_endpoint(host, port) end

return core
