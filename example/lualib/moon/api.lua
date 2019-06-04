---@class core
local core = require("moon_core")
if core then
    return core
end

--------------------FOR EMMYLUA HINT---------------------

--- for luacheck warn: unused params
local ignore_param = print

core = {}

---Remove a timer.<br>
---@param timerid int
function core.remove_timer(timerid)
    ignore_param(timerid)
end

---Return system second.<br>
---@return int
function core.second()
    -- body
end

---Return system millsecond.<br>
---@return int
function core.millsecond()
    -- body
end

---Return system microsecond.<br>
---@return int
function core.microsecond()
    -- body
end

---offset[+,-] server time(millsecond)<br>
function core.time_offset()
    -- body
end

---Thread sleep millsecond.<br>
---@param milliseconds int
function core.sleep(milliseconds)
    ignore_param(milliseconds)
end

---@return string
function core.name()
    -- body
end

---@return int
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
---@param receiver int
---@param cacheid int
---@param header string
---@param sessionid int
---@param type int
function core.send_prefab(receiver, prefabid, header, sessionid, type)
    ignore_param(receiver, prefabid, header, sessionid, type)
end

---@param name string
function core.remove_component(name)
    ignore_param(name)
end

---获取lua虚拟机占用的内存(单位byte)
---@return int
function core.memory_use()
    -- body
end

---获取worker线程数
---@return int
function core.workernum()
    -- body
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
---@return int
function core.now()
    -- body
end

---@class core.message
local message = {}
ignore_param(message)

---获取消息发送者的serviceid
---@return int
function message:sender()
    ignore_param(self)
end

---获取responseid，用于send response模式
---@return int
function message:sessionid()
    ignore_param(self)
end

---获取消息接收者的serviceid
---@return int
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
---@return int
function message:type()
    ignore_param(self)
end

---获取消息的子类型，暂时只有PTYPE_SOCKET类型的消息用到
---local socket_connect = 1
---local socket_accept =2
---local socket_recv = 3
---local socket_close =4
---local socket_error = 5
---@return int
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
---@return int
function message:size()
    ignore_param(self)
end

---对消息数据进行切片，返回从pos(从0开始)开始len个字节的数据(string)。len=-1
---从pos开始的所有数据。
---@param pos int
---@param count int
---@return string
function message:substr(pos, count)
    ignore_param(self, pos, count)
end

---返回消息数据的lightuserdata指针(moon::buffer*)
---@return userdata
function message:buffer()
    ignore_param(self)
end

---更改消息的接收者服务id,框架底层负责把消息转发。同时可以设置消息的header.
---@param header string
---@param receiver int
---@param mtype int
function message:redirect(header, receiver, mtype)
    ignore_param(self, header, receiver, mtype)
end

---@param sender int
---@param header string
---@param receiver int
---@param sessionid int
---@param mtype int
function message:resend(sender, header, receiver, sessionid, mtype)
    ignore_param(self, sender, header, receiver, sessionid, mtype)
end

---@class fs
local fs = {}
ignore_param(fs)

---广度递归遍历一个目录
---local fs = require("fs")
---example 遍历D:/Test目录
---fs.traverse_folder("D:/Test",0,function (path,isdir)
---    --filepath 完整的路径/文件
---    if not isdir then
---        print("File:"..path)
---        print(fs.parent_path(path))
---        print(fs.filename(path))
---        print(fs.extension(path))
---        print(fs.root_path(path))
---        print(fs.stem(path))
---    else
---        print("Path:"..path)
---    end
---end)
---@param dir string 路径
---@param depth int 遍历子目录的深度，0表示 dir 同级目录
---@param callback function 回掉function(path,isdir) end
function fs.traverse_folder(dir, depth, callback)
    ignore_param(dir, depth, callback)
end

---@param dir string
function fs.exists(dir)
    ignore_param(dir)
end

---@param dir string
function fs.create_directory(dir)
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

---@class socketcore
local socketcore = {}
ignore_param(socketcore)

---param protocol moon.PTYPE_TEXT、moon.PTYPE_SOCKET、moon.PTYPE_SOCKET_WS、
---@param host string
---@param port int
---@param protocol int
function socketcore.listen(host, port, protocol)
    ignore_param(host, port, protocol)
end

---param T string 、 moon.buffer
---@param fd int
---@param data T
---@return bool
function socketcore.write(fd, sessionid, owner)
    ignore_param(fd, sessionid, owner)
end

---@param fd int
---@param m core.message
---@return bool
function socketcore.write_message(fd, m)
    ignore_param(fd, m)
end

---param t 秒
---@param fd int
---@param t int
---@return bool
function socketcore.settimeout(fd, t)
    ignore_param(fd, t)
end

---@param fd int
---@return bool
function socketcore.setnodelay(fd)
    ignore_param(fd)
end

---@param fd int
---@param flag string
---@return bool
function socketcore.set_enable_frame(fd, flag)
    ignore_param(fd,flag)
end

---@param fd int
function socketcore.close(fd)
    ignore_param(fd)
end

return core
