error("DO NOT REQUIRE THIS FILE")

---@class core
---@field public id integer @service's id
---@field public name string @service's name
---@field public timezone integer @server's local timezone
local core = {}

---same as os.clock()
---@return number @in seconds
function core.clock()
    -- body
end

---string's md5.
---@param data string
---@return string
function core.md5(data)

end

---convert c-string(char* and size) to lua-string
---@param sz userdata @ lightuserdata char*
---@param len integer
---@return string
function core.tostring(sz, len)

end

---@return string @get server info
function core.server_info()

end

---if linked mimalloc, call mimalloc collect
---@param force boolean @
function core.collect(force)

end

-- ---create a timer
-- ---@param interval integer @ms
-- ---@return integer @timer id
-- function core.timeout(interval)
--     ignore_param(interval)
-- end

--- print console log
function core.log(loglv,...)

end

--- set log level
---@param lv string @DEBUG, INFO, WARN, ERROR
---@return integer
function core.set_loglevel(lv)

end

--- get log level
---@return integer
function core.get_loglevel()
end

--- get this service's cpu cost time
---@return integer
function core.cpu()
    -- body
end

---make a prefab message
---@param data string|userdata
---@return integer @prefab id
function core.make_prefab(data)

end

---send prefab message
---@param receiver integer
---@param prefabid integer
---@param header string
---@param sessionid integer
---@param type integer
function core.send_prefab(receiver, prefabid, header, sessionid, type)

end

-- --- send message from `sender` to `receiver`
-- ---@param receiver integer
-- ---@param data string|userdata @ message
-- ---@param header string
-- ---@param sessionid integer
-- function core.send(receiver, data, header, sessionid)
--     ignore_param(receiver, data, header, sessionid)
-- end

--- remove a service
function core.kill(addr, sessionid)

end

--- query **unique** service's address by name
function core.queryservice(name)

end

---@param key string
---@param value string
function core.set_env(key, value)

end

---@param key string
---@return string
function core.get_env(key)

end

---get worker thread info
---@return string
function core.wsate(workerid)

end

--- let server exit: exitcode>=0 will wait all services quit.
---@param exitcode integer
function core.exit(exitcode)

end

--- get total services's count()
---@return integer
function core.size()
    -- body
end

--- adjusts server time(millsecond)
---@param milliseconds integer
function core.adjtime(milliseconds)

end

-- --- set lua callback
-- ---@param fn fun(msg:userdata,ptype:integer)
-- function core.callback(fn)
--     ignore_param(fn)
-- end

--- Get server timestamp in milliseconds
--- @return integer
function core.now()

end

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
---@param msg userdata @message* lightuserdata
---@param pattern string
function core.decode(msg, pattern)

end

---clone message, but share buffer field
---@param msg userdata @ message* lightuserdata
---@return userdata
function core.clone(msg)

end

---release clone message
---@param msg userdata @ message* lightuserdata
function core.release(msg)

end

---redirect a message to other service
function core.redirect(msg, header, receiver, mtype, sender, sessionid)

end

---get error log count
function core.error_count()
end

---@class asio
local asio = {}

---@param host string
---@param port integer
function asio.try_open(host, port)

end

---param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
---@param host string
---@param port integer
---@param protocol integer
function asio.listen(host, port, protocol)

end

---send data to fd, data string or userdata moon.buffer*
---@param fd integer
---@param data string|userdata
---@param flag integer|nil
---@return boolean
function asio.write(fd, data, flag)

end

---@param fd integer
---@param m userdata @ message*
---@return boolean
function asio.write_message(fd, m)

end

---@param fd integer
---@param m userdata @ message*
---@return boolean
function asio.write_message2(fd, m)

end

---@param fd integer
---@param t integer 秒, 0不检测超时, 默认是0。
---@return boolean
function asio.settimeout(fd, t)

end

---@param fd integer
---@return boolean
function asio.setnodelay(fd)

end

---对于2字节大端长度开头的协议, 通过拆分，来支持收发超过2字节的数据。可以单独控制read,write "r", "w", "wr"
---@param fd integer
---@param flag string
---@return boolean
function asio.set_enable_chunked(fd, flag)

end

---set send queue limit
---@param fd integer @ fd
---@param warnsize integer @ if send queue size > warnsize, print warnning log
---@param errorsize integer @ if send queue size > errorsize, print error log and close socket
function asio.set_send_queue_limit(fd, warnsize, errorsize)
end

---@param fd integer
function asio.close(fd)

end

---@param fd integer
---@return string @ format ip:port
function asio.getaddress(fd)

end

---@param fd integer
---@param addr string @ addr bytes string. see udp callback function second param or user make_endpoint
---@param data string|userdata
---@return boolean
function asio.sendto(fd, addr, data)

end

---@param fd integer
---@param host string
---@param port integer
---@return boolean
function asio.udp_connect(fd, host, port)

end

---@param host string
---@param port integer
---@return string @addr bytes string
function asio.make_endpoint(host, port)

end

return core
