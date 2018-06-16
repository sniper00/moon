# Moon
一个轻量级的游戏服务器框架，底层采用C++17编写,逻辑层采用Lua脚本。

## Dependencies
- [asio-1.10.6](https://github.com/chriskohlhoff/asio)(without boost)
- [lua云风修改版](https://github.com/cloudwu/skynet/tree/master/3rd/lua) 
- [sol2](https://github.com/ThePhD/sol2)(a C++ library binding to Lua)
- [rapidjson](https://github.com/Tencent/rapidjson)
## Bulid

git clone <https://github.com/sniper00/MoonNetLua.git> get the Sources

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
    #premake5.exe --os=linux gmake (in windows platform)
    premake5 gmake
    make config=debug_linux
    make config=release_linux
```
**把编译完成后把bin目录的moon可执行文件拷贝到example目录。**

## Example

### Echo Example
网络通信和websocket示例
- 命令行输入 `./moon 1`运行服务器
- 2字节大端表示长度的协议：python 运行echoclient.py 脚本
- websocket： wesocket_client.html

### Redis Client Example
非阻塞redis client, 默认连接 127.0.0.1 6379
- 命令行输入 `./moon 2`

### Service Send/Call Message
服务间发送消息示例
- 命令行输入 `./moon 3`
- 命令行输入 `./moon 4` call示例

### Cluster Example
服务间发送消息示例
- 命令行输入 `./moon 5`
- 命令行输入 `./moon 6`

### Socket API Example
用Socket API和协程实现的简单HttpServer示例
- 命令行输入 `./moon 7`


QQ交流群543833695

[see more](https://github.com/sniper00/MoonNetLua/wiki)