# Moon
一个轻量级的游戏服务器框架，底层采用C++11编写,逻辑层采用Lua脚本。

## Dependencies
- [asio-1.10.6](https://github.com/chriskohlhoff/asio)(without boost)
- [lua云风修改版](https://github.com/cloudwu/skynet/tree/master/3rd/lua) 
- [sol2](https://github.com/ThePhD/sol2)(a C++ library binding to Lua)
- [rapidjson](https://github.com/Tencent/rapidjson)
## Bulid

git clone <https://github.com/sniper00/MoonNetLua.git> get the Sources

The project use of C++11/14 features, I tested compliers are: 
- GCC 7.2
- Visual Studio 2017 Community Update 5

Linux Platform: 
- make config=debug_linux
- make config=release_linux

**把编译完成后把bin目录的moon可执行文件拷贝到example目录。**

## Example

### Echo Example
网络通信示例
- 命令行输入 `./moon 1`
- python 运行echoclient.py 脚本

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

QQ交流群543833695

[see more](https://github.com/sniper00/MoonNetLua/wiki)