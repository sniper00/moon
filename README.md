# Moon Game Framework
moon 是一个轻量级的游戏服务器框架，底层采用C++编写，逻辑层主要采用Lua。框架使用多线程和多LuaVM(lua service)的模式，
把不同的游戏服务器模块(通常是一个进程)，尽量集中到一个进程中，简化开发和维护难度。一个lua service可以表示一个游戏服务器模块，它们之间通过消息队列互相通信(大部分相情况下只是传递一个数据指针)，通常比进程间IO通信高效，也不需要维护复杂的网络链接。
Lua的协程特性，也使异步逻辑的编写更加简单。

框架本身没有提供太多的游戏业务层逻辑，主要目标是提供基础功能。

如果有疑问或者建议欢迎提交issue,或者加入QQ群
QQ交流群: 543833695

# 目录
- [依赖](#依赖)
- [主要特性](#主要特性)
- [编译](#编译)
- [Hello World](#Hello-World)
- [示例](#示例)
    - [网络-websocket协议](#网络-websocket协议)
    - [动态创建服务](#动态创建服务)
    - [服务间通信-Callback模式](#服务间通信-Callback模式)
    - [服务间通信-协程模式](#服务间通信-协程模式)
    - [定时器](#定时器)
    - [使用配置文件的示例](#使用配置文件的示例)
- [游戏示例](#游戏示例)

## 依赖

- [asio-1.12.1](https://github.com/chriskohlhoff/asio)(without boost)
- [lua云风修改版](https://github.com/cloudwu/skynet/tree/master/3rd/lua) 
- [sol2](https://github.com/ThePhD/sol2)(a C++ library binding to Lua)
- [rapidjson](https://github.com/Tencent/rapidjson)

## 主要特性
- **async** lua协程封装异步操作，取代传统的回调模式
  
- **timer** 轮式定时器的实现，配合lua协程，更加方便处理定时任务
  
- **coroutine socket**  协程socket的封装，方便编写自定义协议的网络模块
  
- **websocket** 支持websocket协议
  
- **cluster**   提供集群间通信（暂时通过统一的配置文件作为服务发现，不支持动态增删)
  
- **extensible**    利用```sol2```库可以方便编写```C/C++```、```lua```交互的扩展模块


## 编译

1. **需要支持C++17的编译器**。[不同平台安装支持C++17的编译器](https://github.com/sniper00/moon/wiki/%E5%AE%89%E8%A3%85CPP17%E7%BC%96%E8%AF%91%E5%99%A8)

2. 获取源码
```
    git clone https://github.com/sniper00/moon.git
``` 

3. 获取最新的premake5。[下载链接](https://premake.github.io/download.html)。选择自己的平台，把下载好的`premake`可执行文件复制到源码中```premake5.lua```同级目录。

4. 生成工程文件并编译
- windows
```shell
    premake5.exe vs2017
```

- linux
```shell
    # 默认采用gcc编译，中括号表示可选项: clang
    ./premake5 gmake [--cc=clang]
    # 默认debug版本，中括号表示可选项: release版本
    make clean [config=release]
    make [config=release]
```

- macosx
```shell
    # 只支持使用clang编译
    ./premake5 gmake --cc=clang
    # 默认debug版本，中括号表示可选项: release版本
    make clean [config=release]
    make [config=release]
```

5. 运行
```shell
    # 直接用一个文件作为lua服务启动，方便快速测试
    ./moon -f lua_service_file
    # 采用配置文件启动，方便配置和管理大量服务，用于正式部署
    ./moon [-c config-file] -r server-id
```

## Hello World
下面编写一个`echo server`功能的`lua service`:

```lua
local moon = require("moon")

local socket = require("moon.socket")

local HOST = "127.0.0.1"
local PORT = 9526

-------------------2 bytes len (big endian) protocol------------------------
socket.on("accept",function(fd, msg)
    print("accept ", fd, msg:bytes())
    socket.settimeout(fd, 10)
end)

socket.on("message",function(fd, msg)
    --echo message to client
    socket.write(fd, msg:bytes())
end)

socket.on("close",function(fd, msg)
    print("close ", fd, msg:bytes())
end)

socket.on("error",function(fd, msg)
    print("error ", fd, msg:bytes())
end)

local listenfd = 0

moon.start(function()
    listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET)
    socket.start(listenfd)--start accept
    print("server start ", HOST, PORT)
    print("enter 'CTRL-C' stop server.")
end)

moon.destroy(function()
    socket.close(listenfd)
end)


```

```shell
# 运行 echo server
./moon -f example/helloworld.lua
# 另启动一个终端运行 client(协程socket编写的客户端)
./moon -f example/helloworld_client.lua
# 输入任意字符
```

## 示例

示例代码可以在 **example** 目录找到


```shell
# 运行 server
./moon -f example/network.lua
# 另启动一个终端运行 client
./moon -f example/helloworld_client.lua
# 输入任意字符

```

### 网络-websocket协议

**network_websocket.lua**

```lua
--Echo Server Example
local moon = require("moon")

local socket = require("moon.socket")

local conf = ...

local HOST = conf.host or "127.0.0.1"
local PORT = conf.port or 12346

socket.wson("accept",function(fd, msg)
    print("wsaccept ", fd, msg:bytes())
    -- 设置read超时，单位秒
    socket.settimeout(fd, 10)
end)

socket.wson("message",function(fd, msg)
    -- binary frame
    -- socket.write(fd, msg)
    -- text frame
    socket.write_text(fd, msg:bytes())
end)

socket.wson("close",function(fd, msg)
    print("wsclose ", fd, msg:bytes())
end)

socket.wson("error",function(fd, msg)
    print("wserror ", fd, msg:bytes())
end)

socket.wson("ping",function(fd, msg)
    print("wsping ", fd, msg:bytes())
    socket.write_pong(fd,"my pong")
end)

local listenfd = 0
moon.start(function()
    listenfd = socket.listen(HOST, PORT, moon.PTYPE_SOCKET_WS)
    socket.start(listenfd)
    print("websocket server start ", HOST, PORT)
    print("enter 'CTRL-C' stop server.")
end)

moon.destroy(function()
    socket.close(listenfd)
end)


```

```shell
./moon -f example/network_websocket.lua
# 使用浏览器运行websocket_client.html
```

### 动态创建服务

**create_service.lua**

```lua
local moon = require("moon")

local conf = ...

if conf.slave then
    local command = {}

    command.QUIT = function ()
        print("recv quit cmd, bye bye")
        moon.quit()
    end

    local function docmd(sender,header,...)
    -- body
        local f = command[header]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(header)))
        end
    end

    moon.start(function()
        print("conf:", conf.message)

        moon.dispatch('lua',function(msg,p)
            local sender = msg:sender()
            local header = msg:header()
            docmd(sender,header, p.unpack(msg))
        end)

        if conf.auto_quit then
            print("auto quit, bye bye")
            -- 使服务退出
            moon.quit()
        end
    end)
else
    moon.start(function()

        -- 动态创建服务, 配置同时可以用来传递一些信息
        moon.new_service("lua", {
            name = "create_service",
            file = "create_service.lua",
            message = "Hello create_service",
            auto_quit = true
        })

        moon.async(function()
            -- 动态创建服务，协程方式等待获得服务ID，方便用来通信
            local serviceid =  moon.co_new_service("lua", {
                name = "create_service",
                file = "create_service.lua",
                slave = true,
                message = "Hello create_service_coroutine"
            })

            print("new service",string.format("%X",serviceid))

            moon.send("lua", serviceid, "QUIT")

            moon.abort()
        end)
    end)

end

```

```shell
 ./moon.exe -f example/example_create_service.lua
```

### 服务间通信-Callback模式

**example_callback.lua**


```lua
local moon = require("moon")

local conf = ...

if conf.receiver then
    -----------------------------THIS IS RECEIVER SERVICE-------------------
    local command = {}

    command.PING = function(sender, ...)
        print(moon.name(), "recv ", sender, "command", "PING")
        print(moon.name(), "send to", sender, "command", "PONG")
        moon.send('lua', sender,'PONG', ...)
    end

    local function docmd(sender,header,...)
        -- body
        local f = command[header]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(header)))
        end
    end

    moon.dispatch('lua',function(msg,p)
        local sender = msg:sender()
        local header = msg:header()
        docmd(sender,header, p.unpack(msg))
    end)

    print("callback example: service receiver start")
else
    -----------------------------THIS IS SENDER SERVICE-------------------
    local command = {}

    command.PONG = function(...)
        print(...)
        print(moon.name(), "recv ", "command", "PING")
        moon.abort()
    end

    local function docmd(header,...)
          local f = command[header]
          if f then
              f(...)
          else
              error(string.format("Unknown command %s", tostring(header)))
          end
    end

    moon.dispatch('lua',function(msg,p)
        local header = msg:header()
        docmd(header, p.unpack(msg))
    end)

    moon.start(function()
        print("callback example: service sender start")

        moon.async(function()
            local receiver =  moon.co_new_service("lua", {
                name = "callback_receiver",
                file = "example_callback.lua",
                receiver = true
            })

            print(moon.name(), "send to", receiver, "command", "PING")
            moon.send('lua', receiver,"PING","Hello")
        end)
    end)
end

```

```shell
 ./moon.exe -f example/example_callback.lua
```

### 服务间通信-协程模式

**example_coroutine.lua**

```lua
local moon = require("moon")
local conf = ...

if conf.receiver then
    local command = {}

    command.PING = function(sender,sessionid, ...)
        print(moon.name(), "recv ", sender, "command", "PING")
        print(moon.name(), "send to", sender, "command", "PONG")
        -- 把sessionid发送回去，发送方resume对应的协程
        moon.response("lua",sender,sessionid,'PONG', ...)
    end

    local function docmd(sender,sessionid,cmd,...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,sessionid,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end

    moon.dispatch('lua',function(msg,p)
        local sender = msg:sender()
        -- sessionid 对应表示发送方 挂起的协程
        local sessionid = msg:sessionid()
        docmd(sender,sessionid, p.unpack(msg))
    end)

else
    moon.start(function()
        moon.async(function()
            local receiver =  moon.co_new_service("lua", {
                name = "example_coroutine",
                file = "example_coroutine.lua",
                receiver = true
            })

            print(moon.name(), "call ", receiver, "command", "PING")
            print(moon.co_call("lua", receiver, "PING", "Hello"))
            moon.abort()
        end)
    end)
end

```

```shell
 ./moon.exe -f example/example_coroutine.lua
```

### 定时器

**example_timer.lua**

```lua
local moon = require("moon")

moon.start(function()

    local count = 10
    moon.repeated(1000,count,function()
        print("repeate 10 times timer tick",count)
        count = count - 1
        if 0 == count then
            moon.abort()
        end
    end)

    moon.async(function()
        print("coroutine timer start")
        moon.sleep(1000)
        print("coroutine timer tick 1 seconds")
        moon.sleep(1000)
        print("coroutine timer tick 1 seconds")
        moon.sleep(1000)
        print("coroutine timer tick 1 seconds")
        moon.sleep(1000)
        print("coroutine timer tick 1 seconds")
        moon.sleep(1000)
        print("coroutine timer tick 1 seconds")
        print("coroutine timer end")
    end)
end)

```

```shell
 ./moon.exe -f example/example_timer.lua
```

### 使用配置文件的示例

**config.json(example 目录)**

**配置文件说明，请参考[wiki](https://github.com/sniper00/moon/wiki/Config)**

#### 测试用例
```shell
    ./moon -c example/config.json -r 1
```

#### Service Send Benchmark
服务间发送消息性能测试
```shell
    ./moon -c example/config.json -r 2
```

#### 协程socket性能测试
`协程socket`编写的多线程服务端，采用`redis-benchmark`测试性能
```shell
    ./moon -c example/config.json -r 3
```

#### Socket性能测试
`2字节大端表示长度的`协议的网络性能测试
```shell
    ./moon -c example/config.json -r 4
```

#### 编写一个Mysql服务,为其他服务提供访问Mysql DB的功能
```shell
    ./moon -c example/config.json -r 5
```

#### 编写一个Redis服务,为其他服务提供访问Redis DB的功能
```shell
    ./moon -c example/config.json -r 6
```

#### sharetable 更新配表示例
```shell
    ./moon -c example/config.json -r 7
```

#### Cluster Example
进程间通信示例
```shell
    ./moon -c example/config.json -r 8
    ./moon -c example/config.json -r 9
```

## 游戏示例
- 简单示例Demo(BallGame): https://github.com/sniper00/BallGame.git

# Friend Open Source
**NoahGameFrame**  
-  Author: ketoo  
-  GitHub: https://github.com/ketoo/NoahGameFrame  
-  Description: A fast, scalable, distributed game server framework for C++, include actor library, network library,can be used as a real time multiplayer game engine ( MMO RPG ), which plan to support C#/Python/Lua script, and support Unity3d, Cocos2dx, FlashAir client access.  
