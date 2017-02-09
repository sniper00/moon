# Moon
一个轻量级的游戏服务器框架，底层采用C++11编写,逻辑层采用Lua脚本。

## Dependencies
- [asio-1.10.6](https://github.com/chriskohlhoff/asio)(without boost)
- lua5.3
- [sol2](https://github.com/ThePhD/sol2)(a C++ library binding to Lua)

## Bulid

git clone <https://github.com/sniper00/MoonNetLua.git> get the Sources

The project use of C++11/14 features, I tested compliers are: 
- GCC 5.4 
- Visual Studio 2015 Community Update 4

Linux Platform: 
- make config=debug_linux
- make config=release_linux

## example
把编译完成后把bin目录的moon可执行文件，netcore、lua53 动态库拷贝到resources目录，直接运行moon可执行文件，开启EchoServer。
- lua 用lua启动器运行EchoClient.lua,需要luasocket库
- python 运行EchoClient.py 脚本


QQ交流群543833695

[see more](https://github.com/sniper00/MoonNetLua/wiki)
