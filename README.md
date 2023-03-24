# Current building status

[![linux-gcc](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml)

[![linux-clang](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml)

[![macos-clang](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml)

[![windows-vs2022](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml)

# Moon
Moon is a lightweight game server framework based on the actor model. One worker thread can have one or multiple actors (services), which communicate with each other through message queues. There are many features for game server development:

- Less core code, easy to learn
- Cross-platform (Windows, Linux, MacOS)
- Optimized networking
   - Tcp
   - Udp/Kcp
   - Websockets
   - Http
- Asynchronous based on Lua coroutines
   - Coroutine-socket
   - Timer
   - Inter-service communication
   - Inter-process communication
   - Redis/PostgreSQL/Mongodb/Mysql async client driver
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

[Download Pre-Built Binaries](https://github.com/sniper00/moon/releases)



```shell
# Run test
./moon test/main_test.lua

# Run script in the example directory, e:
./moon example/example_timer.lua

# This is a guessing game, example/GuessGame.md
./moon example/main_game.lua
```

# Demo
- [MoonDemo](https://github.com/sniper00/MoonDemo.git)

# Build

1. Make sure your compiler(msvc2019, gcc 9.3+, clang 9.0+) support C++17 or [Install `C++17` Compiler.](https://github.com/sniper00/moon/wiki/Build#%E5%AE%89%E8%A3%85c17%E7%BC%96%E8%AF%91%E5%99%A8)。

2. Clone source code 

```
    git clone --recursive https://github.com/sniper00/moon.git
``` 

3. Build
    - windows run `build.bat`
    - linux:`make config=release`
    - maxos: [See detail build steps](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)


4. If you want modify premake5 script, [See detail build steps](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)

# Friend Open Source
- [NoahGameFrame](https://github.com/ketoo/NoahGameFrame)
- [hive](https://github.com/hero1s/hive)
