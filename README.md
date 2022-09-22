# Current building status

[![linux-gcc](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml)

[![linux-clang](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml)

[![macos-clang](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml)

[![windows-vs2022](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml)

# Demo
- [BallGame](https://github.com/sniper00/BallGame.git)

# Moon
Moon is a lightweight online game server framework implement with actor model. One thread may have 1-N luaVM, they use message queue communication. There are many features for game server development:

- Less core code, easy to learn
- Cross-platform (Windows, Linux, MacOS)
- Optimized networking
   - tcp
   - websockets
   - http
- Lua coroutine based asynchronous
   - coroutine-socket
   - timer
   - inter luaVM communication
   - inter cluster(process) communication
   - redis/mysql/pgsql/mongodb async driver
- High performance and optimized Lua Json library
- Lua protobuf library
- Lua filesystem
- Recast Navigation
# OverView
![image](https://github.com/sniper00/MoonNetLua/raw/master/image/02.png)

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
# run echo server
./moon example/helloworld.lua
# another terminal run client(coroutine socket writed)
./moon example/helloworld_client.lua
# input any string
```

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
