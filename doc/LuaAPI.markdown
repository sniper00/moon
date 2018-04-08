
<h1>Lua API</h1>

<h2 id="1">class message</h2>
message 主要用于服务间消息传递，在lua代码中访问到的都是message指针，所以lua中不能保存传递进来的message对象。

**Lua代码中不能创建message对象**

- `sender()` 获取消息发送者的serviceid(number)
- `receiver()` 获取消息接收者的serviceid(number)
- `type()` 获取消息类型(number)，用来区分消息协议
```lua
local PTYPE_SYSTEM = 1 --框架内部消息
local PTYPE_TEXT = 2 --字符串消息
local PTYPE_LUA = 3 --lua编码的消息
local PTYPE_SOCKET = 4 --网络消息
local PTYPE_ERROR = 5 --错误消息
```
- `subtype()`获取消息的子类型，暂时只有PTYPE_SOCKET类型的消息用到
```lua
local socket_connect = 1
local socket_accept =2
local socket_recv = 3
local socket_close =4
local socket_error = 5
local socket_logic_error = 6
```
- `header()` 获取消息header(string).消息头和消息数据分开存储，大多情况下只用解析header来处理消息，消息不用更改，方便用于广播数据。

- `bytes()` 获取消息数据(string)
- `size()` 获取消息数据长度(number)
- `subbytes(pos,len)` 对消息数据进行切片，返回从pos(从0开始)开始len个字节的数据(string)
- `buffer()` 返回消息数据的userdata 指针
- `redirect(header,receiver)` 更改消息的接收者服务id,框架底层负责把消息转发。同时可以设置消息的header.
- `responseid()` 取responseid，用于send response模式


<h2 id="2">service functions</h2>

- `name()` 获取服务的名字(string)
- `sid()` 获取服务的id(number)
- `make_cache(string)` 生成缓存消息,返回缓存id(number),用于广播，减少数据拷贝。缓存消息只在当前调用堆栈有效。
- `send_cache(receiver,cacheid,header,responseid,type)` 根据cacheid发送缓存消息
- `add_component_tcp(name)` 给服务器添加一个tcp网络组件，返回组件的指针moon::tcp*
- `get_component_tcp(name)` 根据name获取已经添加的tcp网络组件，返回组件的指针moon::tcp*
- `set_init(function)` 设置服务初始化回掉函数，回掉函数需要返回bool, true 表示初始化成功，false失败。在回掉函数里和初始化服务自身的相关信息，不能有协程相关操作。
- `set_start(function)` 设置服务启动回掉函数,此时unique service 已经初始化完毕，可以收发信息。
- `set_exit(function)` 设置进程收到进程退出时的回掉函数，可以在此处理进程退出前的相关操作，如保存数据，最后必须要调用 removeself().
- `set_dispatch(function) ` 设置消息处理回掉函数
- `set_destroy(function)` 设置服务销毁时回掉函数，不要有异步操作，回掉函数里的所有异步操作都将失效。
- `memory_use()` 获取lua虚拟机占用的内存byte
- `send(sender,receiver,data,header,responseid,type)` 向某个服务发送消息。参数含义同message
- `new_service(stype, config, unique, shared, workerid)` 创建服务
- `remove_service(sid, bresponse)` 移除一个服务
- `removeself()` 移除当前服务
- `unique_service()` 根据服务name获取服务id,注意只能查询创建时unique配置为true的服务
- `start_coroutine(function)` 移动一个协程
- `repeated(mills, times, cb)` 定时器
- `co_wait(mills)` 定时器的协程封装
- `co_remove_service(sid)` 移除一个服务的协程封装
- `co_call(PTYPE, receiver, ...)` 请求回应模式的协程封装
- `response(PTYPE, receiver, responseid, ...)` 回应消息，一般配合co_call使用
- `register_protocol(t)` 注册某个类型的消息的 编码解码，和消息处理回掉
- `millsecond()` 获取当前毫秒级时间

# path
跨平台的路径操作

 - `traverse_folder(path,pathDepth,callback)` 根据深度遍历一个目录。path 路径。pathDepth 路径深度，设置遍历子目录的深度（0 path同级目录）。callback 回掉。 no return。
```lua
--example 遍历D:/Test目录
    local timerID = 0
    path.traverse_folder("D:/Test",0,function (filepath,filetype)
        --filepath 完整的路径/文件
        --filetype 0目录，1 文件
        if filetype == 1 then
            print("File:"..filepath)
        else
            print("Path:"..filepath)
        end
    end)
```
 - `exist(path)` 判断一个路径是否存在
 - `create_directory(path)` 如果目录不存在，创建完整的目录
 - `directory(filepath)` 根据文件路径，获取路径
 - `extension(filepath)` 获取文件的扩展名
 - `filename(filepath)` 根据文件路径，获取文件名
 - `name_without_extension(filepath)` 同上，获取不带扩展名的文件名
 - `current_directory()`获取当前工作目录


# class tcp
- `close(sessionid)` 关闭某个连接
- `send(sessionid, data)` 向某个连接发送数据， data（string）
- `send_message(sessionid,msg)` 向某个连接发送 message
- `setprotocol(pt)` 设置协议类型
- `settimeout(second)` 设置连接read超时
- `setnodelay(connid)`

## socket 的协程封装
参见 lualib/moon/socket.lua
