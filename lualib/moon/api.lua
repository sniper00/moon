---@class core
local core = {}

--------------------FOR EMMYLUA HINT---------------------

--- for luacheck warn: unused params
local ignore_param = print

core = {}

---Remove a timer.<br>
---@param timerid integer
function core.remove_timer(timerid)
    ignore_param(timerid)
end

---Return system second.<br>
---@return integer
function core.second()
    -- body
end

---Return system millsecond.<br>
---@return integer
function core.millsecond()
    -- body
end

---Return system microsecond.<br>
---@return integer
function core.microsecond()
    -- body
end

---推进时间 server time(millsecond)
---@param milliseconds integer
function core.adjtime(milliseconds)
    ignore_param(milliseconds)
end

---Thread sleep millsecond.<br>
---@param milliseconds integer
function core.sleep(milliseconds)
    ignore_param(milliseconds)
end

---@return string
function core.name()
    -- body
end

---返回当前service id
---@return integer
function core.id()
    -- body
end

---更改消息的接收者服务id,框架底层负责把消息转发。同时可以设置消息的header.
---@param header string
---@param receiver integer
---@param mtype integer
function core.redirect(msg, header, receiver, mtype)
    ignore_param(msg, header, receiver, mtype)
end

---生成缓存消息,返回缓存id(number),用于广播，减少数据拷贝。缓存消息只在当前调用有效。
---@overload fun(buf:string)
---@param buf userdata
function core.make_prefab(buf)
    ignore_param(buf)
end

---根据缓存id发送缓存消息
---@param receiver integer
---@param prefabid integer
---@param header string
---@param sessionid integer
---@param type integer
function core.send_prefab(receiver, prefabid, header, sessionid, type)
    ignore_param(receiver, prefabid, header, sessionid, type)
end

---@param key string
---@param value string
function core.set_env(key, value)
    ignore_param(key, value)
end

---@param key string
---@return string
function core.get_env(key)
    ignore_param(key)
end

---使服务进程退出
function core.abort()
    -- body
end

--- get server time(milliseconds)
---@return integer
function core.now()
    -- body
end

--- get total service count()
---@return integer
function core.service_count()
    -- body
end

--- print error level log
function core.error(...)
    ignore_param(...)
end

---@class message
local message = {}
ignore_param(message)

---获取消息发送者的serviceid
---@return integer
function message:sender()
    ignore_param(self)
end

---获取responseid，用于send response模式
---@return integer
function message:sessionid()
    ignore_param(self)
end

---获取消息接收者的serviceid
---@return integer
function message:receiver()
    ignore_param(self)
end

---获取消息类型,用来区分消息协议
---local PTYPE_SYSTEM = 1 --框架内部消息
---local PTYPE_TEXT = 2 --字符串消息
---local PTYPE_LUA = 3 --lua_serialize编码的消息
---local PTYPE_SOCKET = 4 --网络消息
---local PTYPE_ERROR = 5 --错误消息
---local PTYPE_SOCKET_WS = 6--web socket 网络消息
---@return integer
function message:type()
    ignore_param(self)
end

---获取消息的子类型，暂时只有PTYPE_SOCKET类型的消息用到
---local socket_connect = 1
---local socket_accept =2
---local socket_recv = 3
---local socket_close =4
---local socket_error = 5
---@return integer
function message:subtype()
    ignore_param(self)
end

---获取消息header(string).消息头和消息数据分开存储，大多情况下只用解析header来转发消息，
---消息不用更改，方便用于广播数据。
---@return string
function message:header()
    ignore_param(self)
end

---获取消息数据(string)
---@return string
function message:bytes()
    ignore_param(self)
end

---获取消息数据长度(number)
---@return integer
function message:size()
    ignore_param(self)
end

---对消息数据进行切片，返回从pos(从0开始)开始len个字节的数据(string)。len=-1
---从pos开始的所有数据。
---@param pos integer
---@param count integer
---@return string
function message:substr(pos, count)
    ignore_param(self, pos, count)
end

---返回消息数据的lightuserdata指针(moon::buffer*)
---@return userdata
function message:buffer()
    ignore_param(self)
end

---@param sender integer
---@param header string
---@param receiver integer
---@param sessionid integer
---@param mtype integer
function message:resend(sender, header, receiver, sessionid, mtype)
    ignore_param(self, sender, header, receiver, sessionid, mtype)
end

---@class fs
local fs = {}
ignore_param(fs)

---返回指定的文件夹包含的文件或文件夹的名字的列表
---@param dir string @路径
---@param depth integer @遍历子目录的深度，默认0
---@return table @返回数组
function fs.listdir(dir, depth)
    ignore_param(dir, depth)
end

function fs.isdir(dir)
    ignore_param(dir)
end

function fs.join(dir1, dir2)
    ignore_param(dir1, dir2)
end

---@param dir string
function fs.exists(dir)
    ignore_param(dir)
end

---@param dir string
function fs.create_directory(dir)
    ignore_param(dir)
end

---@param fp string
function fs.remove(fp)
    ignore_param(fp)
end

---@param dir string
function fs.remove_all(dir)
    ignore_param(dir)
end

---@return string
function fs.working_directory()
    -- body
end

---@param fp string
---@return string
function fs.parent_path(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.filename(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.extension(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.root_path(fp)
    ignore_param(fp)
end

---@param fp string
---@return string
function fs.stem(fp)
    ignore_param(fp)
end

---@class asio
local asio = {}
ignore_param(asio)

---param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
---@param host string
---@param port integer
---@param protocol integer
function asio.listen(host, port, protocol)
    ignore_param(host, port, protocol)
end

---send data to fd, data string or userdata moon.buffer*
---@overload fun(fd:integer, data:string)
---@overload fun(fd:integer, data:userdata)
---@return boolean
function asio.write(fd, data)
    ignore_param(fd, data)
end

---@param fd integer
---@param m message
---@return boolean
function asio.write_message(fd, m)
    ignore_param(fd, m)
end

---@param fd integer
---@param t integer 秒, 0不检测超时, 默认是0。
---@return boolean
function asio.settimeout(fd, t)
    ignore_param(fd, t)
end

---@param fd integer
---@return boolean
function asio.setnodelay(fd)
    ignore_param(fd)
end

---@param fd integer
---@param flag string
---对于2字节大端长度开头的协议, 通过拆分，来支持收发超过2字节的数据。可以单独控制read,write "r", "w", "wr"
---@return boolean
function asio.set_enable_chunked(fd, flag)
    ignore_param(fd,flag)
end

---@param fd integer
function asio.close(fd)
    ignore_param(fd)
end

---@class random
local random = {}
ignore_param(random)

---integer rand range [min,max]
---@param min integer
---@param max integer
---@return integer
function random.rand_range(min, max)
    ignore_param(min, max)
end

---integer rand range [min,max],返回范围内指定个数不重复的随机数
---@param min integer
---@param max integer
---@param count integer
---@return table
function random.rand_range_some(min, max, count)
    ignore_param(min, max, count)
end

---double rand range [min,max)
---@param min number
---@param max number
---@return number
function random.randf_range(min, max)
    ignore_param(min, max)
end

---参数 percent (0,1.0),返回bool
---@param percent number
---@return boolean
function random.randf_percent(percent)
    ignore_param(percent)
end

---按权重随机
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@return integer
function random.rand_weight(values, weights)
    ignore_param(values, weights)
end

---按权重随机count个值
---@param values integer[] @要随机的值
---@param weights integer[] @要随机的值的权重
---@param count integer @要随机的个数
---@return table
function random.rand_weight_some(values, weights, count)
    ignore_param(values, weights, count)
end


return core
