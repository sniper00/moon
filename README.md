# Moon Game Framework
moon 是一个轻量级的游戏服务器框架，底层采用C++编写，逻辑层主要采用Lua。框架使用多线程和多LuaVM(lua service)的模式，
把不同的游戏服务器模块(通常是一个进程)，尽量集中到一个进程中，简化开发和维护难度。一个lua service可以表示一个游戏服务器模块，它们之间通过消息队列互相通信(大部分相情况下只是传递一个数据指针)，通常比进程间IO通信高效，也不需要维护复杂的网络链接。
Lua的协程特性，也使异步逻辑的编写更加简单。

框架本身没有提供太多的游戏业务层逻辑，主要目标是提供基础功能。

如果有疑问或者建议欢迎提交issue,或者加入QQ群
QQ交流群: 543833695

## 依赖

- [asio](https://github.com/chriskohlhoff/asio)
- [lua云风修改版](https://github.com/cloudwu/skynet/tree/master/3rd/lua) 
- [sol2](https://github.com/ThePhD/sol2)(A C++ library binding to Lua)
- [rapidjson](https://github.com/Tencent/rapidjson)

## 主要特性
- **async** lua协程封装异步操作，取代传统的回调模式
  
- **timer** 轮式定时器的实现，配合lua协程，更加方便处理定时任务
  
- **coroutine socket**  协程socket的封装，方便编写自定义协议的网络模块
  
- **websocket** 支持websocket协议
  
- **cluster**   提供集群间通信（暂时通过统一的配置文件作为服务发现，不支持动态增删)
  
- **extensible**    利用```sol2```库可以方便编写```C/C++```、```lua```交互的扩展模块

## 快速开始

1. 框架基于`C++17`编写，需要安装相应的编译器，[不同平台安装支持C++17的编译器](https://github.com/sniper00/moon/wiki/Build#%E5%AE%89%E8%A3%85c17%E7%BC%96%E8%AF%91%E5%99%A8)。

2. 获取源码
```
    git clone https://github.com/sniper00/moon.git
``` 

3. 编译
    - windows 双击运行 `build.bat`。
    - linux:
        ```shell
            # 添加可执行权限
            chmod +x build.sh
            chmod +x premake5
            ./build.sh
        ```

    **如果失败，参考[详细编译步骤](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)**。

4. 运行

```shell
# 运行 echo server
./moon -f example/helloworld.lua
# 另启动一个终端运行 client(协程socket编写的客户端)
./moon -f example/helloworld_client.lua
# 输入任意字符
```

## 游戏示例
- 简单示例Demo(BallGame): https://github.com/sniper00/BallGame.git

# Friend Open Source
**NoahGameFrame**  
-  Author: ketoo  
-  GitHub: https://github.com/ketoo/NoahGameFrame  
-  Description: A fast, scalable, distributed game server framework for C++, include actor library, network library,can be used as a real time multiplayer game engine ( MMO RPG ), which plan to support C#/Python/Lua script, and support Unity3d, Cocos2dx, FlashAir client access.  
