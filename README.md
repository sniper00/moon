# Current building status

[![linux-gcc](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-gcc.yml)

[![linux-clang](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/linux-clang.yml)

[![macos-clang](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/macos-clang.yml)

[![windows-vs2022](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml/badge.svg)](https://github.com/sniper00/moon/actions/workflows/windows-vs2022.yml)

[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=sniper00_moon&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=sniper00_moon)

# Moon
Moon is a lightweight game server framework based on the actor model. One worker thread can have one or more actors (services), which communicate with each other through message queues. There are many features for game server development:

- The architecture is simple, and the source code is concise and easy to understand.
- Cross-platform (Windows, Linux, MacOS)
- Uses ASIO for scheduling
- Uses Lua for logic scripting
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

> Want more extension libraries? See https://github.com/sniper00/moon-extensions

# OverView

## Framework

![image](https://github.com/sniper00/MoonNetLua/raw/master/src/image/01.png)

# Community

- [![Gitter](https://badges.gitter.im/undefined/community.svg)](https://gitter.im/undefined/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

- QQ group: 543833695

# Documents
  
  [Doc](https://homrua.com/docs/intro)


# Quick Start

[Download Pre-Built Binaries](https://github.com/sniper00/moon-extensions/releases/tag/prebuilt)



```shell
# Run test
./moon example/test/main_test.lua

# Run script in the example directory, e:
./moon example/example_timer.lua

# This is a guessing game, example/GuessGame.md
./moon example/main_game.lua
```

# Demo
- [MoonDemo](https://github.com/sniper00/MoonDemo.git)



# Build

1. Make sure your compiler(vs2022 17.5+, gcc 9.3+, clang 9.0+) support C++17 or [Install `C++17` Compiler.](https://github.com/sniper00/moon/wiki/Build#%E5%AE%89%E8%A3%85c17%E7%BC%96%E8%AF%91%E5%99%A8)。

2. Install **premake5** and ensure that the **premake5** executable location is included in your system path.

      > Download link: https://premake.github.io/download  
      > On Mac, you can install it using `brew install premake`

2. Clone source code 

    ```shell
      git clone https://github.com/sniper00/moon.git
    ``` 

3. Build

    ```shell
      cd moon
      premake5 build --release
    ```

4. Run
    ```shell
      premake5 run --release example/send_benchmark.lua
    ```

5. Publish
   ```shell
      premake5 publish
   ```

**If you want modify premake5 script, [See detail build steps](https://github.com/sniper00/moon/wiki/Build#%E7%BC%96%E8%AF%91)**


# Use case

### If your project uses moon, feel free to contact the author. The author is happy to showcase your work..

| <img src="https://img.tapimg.com/market/icons/91b7979cd1fc8521f0a1635ec6962885_360.png?imageView2/1/w/270/q/80/interlace/1/ignore-error/1" alt="g1" width="128" height="128" /> | <img src="https://img.tapimg.com/market/lcs/d41a7948d794739454458f2dff4ab5c3_360.png?imageView2/1/w/270/q/80/interlace/1/ignore-error/1" alt="g2" width="128" height="128" />|
| ------------------------------------------------------------ | ------------------------------------------------------------ | 
| <p align="center">[Age of kita](https://www.taptap.cn/app/225455)</p> | <p align="center">[战神遗迹](https://www.taptap.cn/app/194605)</p>|

# Friend Open Source
- [NoahGameFrame](https://github.com/ketoo/NoahGameFrame)
- [hive](https://github.com/hero1s/hive)
