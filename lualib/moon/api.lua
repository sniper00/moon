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
---@param exitcode integer
function core.exit(exitcode)
    ignore_param(exitcode)
end

--- get server time(milliseconds)
---@return integer
function core.now()
    -- body
end

--- get total services's count()
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

---clone message, but share buffer field
---@param msg userdata @ message* lightuserdata
---@return userdata
function message.clone(msg)
    ignore_param(msg)
end

---release clone message
---@param msg userdata @ message* lightuserdata
function message.release(msg)
    ignore_param(msg)
end

---redirect a message to other service
function message.redirect(header, receiver, mtype, sender, sessionid)
    ignore_param(header, receiver, mtype, sender, sessionid)
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
