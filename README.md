# Current building status

[![linux-gcc](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml)

[![linux-clang](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml)

[![macos-clang](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml)

[![windows-vs2022](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml)

# Moon
Moon is a lightweight online game server framework implement with actor model. One thread may have 1-N service(luaVM), they use message queue for communication. There are many features for game server development:

- Less core code, easy to learn
- Cross-platform (Windows, Linux, MacOS)
- Optimized networking
   - Tcp
   - Websockets
   - Http
- Lua coroutine based asynchronous
   - Coroutine-socket
   - Timer
   - Service-to-service communication
   - Interprocess(cluster) communication
   - Redis/Mysql/Pgsql/Mongodb async client driver
- High performance and optimized Lua Json library
- Lua protobuf library
- Lua filesystem
- Recast Navigation
- Lua zset library for ranklist

# OverView

## Framework

![image](https://github.com/sniper00/MoonNetLua/raw/master/image/01.png)

# Community

- [![Gitter](https://badges.gitter.im/undefined/community.svg)](https://gitter.im/undefined/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

- QQ group: 543833695

# Documents
  
  https://github.com/sniper00/moon/wiki

# Dependencies

- [asio](https://github.com/chriskohlhoff/asio)
- [lua](https://github.com/cloudwu/skynet/tree/master/3rd/lua)

# Quick Start

## Download Pre-Built Binaries

https://github.com/sniper00/moon/releases

```shell
# This is an example show put different services in one process
./moon example/main_game.lua

# Run test
./moon test/main_test.lua

# Run script in the example directory, e:
./moon example/example_timer.lua

```

## Demo
- [MoonDemo](https://github.com/sniper00/MoonDemo.git)

## Manual Build

1. Make sure your compiler(msvc2019, gcc 9.3+, clang 9.0+) support for C++17 or [Install `C++17` Compiler.](https://github.com/sniper00/moon/wiki/Build#%E5%AE%89%E8%A3%85c17%E7%BC%96%E8%AF%91%E5%99%A8)。

2. Moon uses [premake5](http://premake.github.io/) to build platform specific projects. Download it and make sure it's available on your path.

3. Clone source code

```
    git clone https://github.com/sniper00/moon.git
``` 

4. If want link mimalloc, edit premake5.lua `local MOON_ENABLE_MIMALLOC = true`

5. Build
    - windows run `build.bat`。
    - linux:
        ```shell
            chmod +x build.sh
            chmod +x premake5
            ./build.sh
        ```
    - macosx:
        ```shell
            ./premake5 gmake --cc=clang
            make clean config=release
            make config=release
        ```

    **If failed，[See detail steps](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)**。

## Friend Open Source
- [NoahGameFrame](https://github.com/ketoo/NoahGameFrame)
- [hive](https://github.com/hero1s/hive)
