# Moon
一个轻量级的游戏服务器框架，底层采用C++17编写,逻辑层采用Lua脚本。

## 主要特性有
- **async** lua协程封装异步操作，取代传统的回掉模式。
  
- **timer** 轮式定时器的实现，配合lua协程，更加方便处理定时任务。
  
- **coroutine socket**  协程socket的封装，方便编写自定义协议的网络模块。
  
- **async redis client**    协程socket封装的redis DB异步客户端。
  
- **websocket** 支持websocket协议(暂时只支持作为服务端)。
  
- **cluster**   提供集群间通信（暂时通过统一的配置文件作为服务发现，不支持动态增删)。
  
- **extensible**    利用```sol2```库可以方便编写```C/C++```、```lua```交互的扩展模块。

## Dependencies
- [asio-1.12.1](https://github.com/chriskohlhoff/asio)(without boost)
- [lua云风修改版](https://github.com/cloudwu/skynet/tree/master/3rd/lua) 
- [sol2](https://github.com/ThePhD/sol2)(a C++ library binding to Lua)
- [rapidjson](https://github.com/Tencent/rapidjson)
## Bulid

git clone <https://github.com/sniper00/moon.git> get the Sources

需要使用premake生成工程文件, [下载最新的premake5](https://premake.github.io/download.html)

The project use of C++17 features, I tested compliers are: 
- GCC 7.2
- Visual Studio 2017 Community Update 5

Windows Platform:
```shell
    premake5.exe vs2017
```

Linux Platform:
```shell
    ./premake5 gmake
    #Debug
    make
    # Release
    # make clean config=release_linux
    # make config=release_linux
    cd example
    ./moon 1
```

## [更多文档](https://github.com/sniper00/moon/wiki)

## QQ交流群543833695


