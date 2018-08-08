function os.winSdkVersion()
    local reg_arch = iif( os.is64bit(), "\\Wow6432Node\\", "\\" )
    local sdk_version = os.getWindowsRegistry( "HKLM:SOFTWARE" .. reg_arch .."Microsoft\\Microsoft SDKs\\Windows\\v10.0\\ProductVersion" )
    if sdk_version ~= nil then return sdk_version end
end

workspace "Server"
    configurations { "Debug", "Release" }

    if os.istarget("windows") then
        platforms { "Win32", "x64"}
        characterset ("MBCS")
        systemversion(os.winSdkVersion() .. ".0")
    else
        platforms {"linux"}

    end

    flags{"NoPCH","RelativeLinks"}
    cppdialect "C++17"

    location "./"
    libdirs{"./libs"}

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter { "platforms:Win32" }
        system "windows"
        architecture "x86"

    filter { "platforms:x64" }
        system "windows"
        architecture "x64"

    filter { "platforms:Linux" }
        system "linux"     

 
project "lua53"
    objdir "obj/lua53/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/lua53"
    kind "SharedLib"
    language "C"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./third/lua53"}
    files { "./third/lua53/**.h", "./third/lua53/**.c"}
    removefiles("./third/lua53/luac.c")
    removefiles("./third/lua53/lua.c")
    postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/"}
    filter { "system:windows" }
        defines {"LUA_BUILD_AS_DLL"}
    filter { "system:linux" }
        defines {"LUA_USE_LINUX"}
        links{"dl"}

project "rapidjson"
    objdir "obj/rapidjson/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/rapidjson"
    kind "StaticLib"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./third","./third/rapidjson","./third/rapidjsonlua"} 
    --links{"lua53"}
    files { "./third/rapidjsonlua/**.hpp", "./third/rapidjsonlua/**.cpp"}
    filter {"system:linux"}
        buildoptions {"-msse4.2"}

project "moon"
    objdir "obj/moon/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/moon"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./","./moon","./moon/core","./third","./clib"}
    files {"./moon/**.h", "./moon/**.hpp","./moon/**.cpp" }
    links{"lua53","rapidjson"}
    defines {
        "ASIO_STANDALONE" ,
        "ASIO_HAS_STD_ARRAY",
        "ASIO_HAS_STD_TYPE_TRAITS",
        "ASIO_HAS_STD_SHARED_PTR",
        "ASIO_HAS_CSTDINT",
        "ASIO_DISABLE_SERIAL_PORT",
        "ASIO_HAS_STD_CHRONO",
        "ASIO_HAS_MOVE",
        "ASIO_HAS_VARIADIC_TEMPLATES",
        "ASIO_HAS_CONSTEXPR",
        "ASIO_HAS_STD_SYSTEM_ERROR",
        "ASIO_HAS_STD_ATOMIC",
        "ASIO_HAS_STD_FUNCTION",
        "ASIO_HAS_STD_THREAD",
        "ASIO_HAS_STD_MUTEX_AND_CONDVAR",
        "ASIO_HAS_STD_ADDRESSOF",

        "SOL_CHECK_ARGUMENTS"
    }
    postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/"}
    filter { "system:windows" }
        defines {"_WIN32_WINNT=0x0601"}
    filter { "system:linux" }
        links{"dl","pthread"} --"-static-libstdc++"
        linkoptions {"-Wl,-rpath=./"}
    filter "configurations:Debug"
        targetsuffix "-d"

-----------------------------------------------------------------------------------
--[[
    Lua C/C++扩展 在下面添加
]]

-- lua版protobuf
project "protobuf"
    objdir "obj/protobuf/%{cfg.platform}_%{cfg.buildcfg}" --编译生成的中间文件目录
    location "build/protobuf" -- 生成的工程文件目录
    kind "SharedLib" -- 静态库 StaticLib， 动态库 SharedLib
    targetdir "bin/%{cfg.buildcfg}" --目标文件目录
    includedirs {"./third/lua53","./third"} --头文件搜索目录
    files { "./third/protobuf/**.h", "./third/protobuf/**.c"} --需要编译的文件， **.c 递归搜索匹配的文件
    links{"lua53"} -- windows 版需要链接 lua 库
    targetprefix "" -- linux 下需要去掉动态库 'lib' 前缀
    language "C++" -- 编译的语言，这个库在widnows下需要用C++来编译
    postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/clib/"} -- 编译完后拷贝到example目录
    filter { "system:windows" }
        defines {"LUA_BUILD_AS_DLL"} -- windows下动态库导出宏定义
        buildoptions {"/TP"} -- windows 下强制用C++编译，默认会根据文件后缀名选择编译

--[[
    lua版mysql,如果需要lua mysql 客户端，取消下面注释.
    依赖： 需要连接 mysql C client库,
    1. windows 下需要设置MYSQL_HOME.
    2. Linux 下需要确保mysql C client头文件目录和库文件目录正确
]]

-- project "mysql"
--     location "build/lualib/mysql"
--     kind "SharedLib"
--     targetdir "bin/%{cfg.buildcfg}"
--     includedirs {"./","./third","./lualib/mysql"}
--     files { "./lualib/mysql/**.hpp","./lualib/mysql/**.h","./lualib/mysql/**.cpp"}
--     targetprefix ""
--     language "C++"
--     links{"lua53"}
--     postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/clib/"} -- 编译完后拷贝到example目录
--     filter { "system:windows" }
--         defines {"LUA_BUILD_AS_DLL"}
--         if os.istarget("windows") then
--             assert(os.getenv("MYSQL_HOME"),"please set mysql environment 'MYSQL_HOME'")
--             includedirs {os.getenv("MYSQL_HOME").. "/include"}
--             libdirs{os.getenv("MYSQL_HOME").. "/lib"} -- 搜索目录
--             links{"libmysql"}
--         end
--     filter { "system:linux" }
--         if os.istarget("linux") then
--             assert(os.isdir("/usr/include/mysql"),"please make sure you have install mysql, or modify the default include path,'/usr/include/mysql'")
--             assert(os.isdir("/usr/lib64/mysql"),"please make sure you have install mysql, or modify the default lib path,'/usr/lib64/mysql'")
--             includedirs {"/usr/include/mysql"}
--             libdirs{"/usr/lib64/mysql"} -- 搜索目录
--             links{"mysqlclient"}
--         end
