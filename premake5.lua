workspace "Server"
    configurations { "Debug", "Release" }

    flags{"NoPCH","RelativeLinks"}
    cppdialect "C++17"

    location "./"
    libdirs{"./libs"}

    if os.istarget("windows") then
        platforms { "Win32", "x64"}
        characterset ("MBCS")
        systemversion "latest"
        filter { "platforms:Win32" }
        architecture "x86"
        warnings "Extra"

        filter { "platforms:x64" }
            architecture "x64"
            warnings "Extra"
    end

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter { "system:linux" }
        warnings "High"

    filter { "system:macosx" }
        warnings "High"

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
    filter { "system:macosx" }
        defines {"LUA_USE_MACOSX"}
        links{"dl"}

project "rapidjson"
    objdir "obj/rapidjson/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/rapidjson"
    kind "StaticLib"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./third","./third/lua53","./third/rapidjsonlua"}
    --links{"lua53"}
    files { "./third/rapidjsonlua/**.hpp", "./third/rapidjsonlua/**.cpp"}
    filter {"system:linux or macosx"}
        buildoptions {"-msse4.2"}
    filter { "system:windows" }
        defines {"WIN32"}

project "moon"
    objdir "obj/moon/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/moon"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./","./moon","./moon/core","./third","./third/lua53"}
    files {"./moon/**.h", "./moon/**.hpp","./moon/**.cpp" }
    links{"lua53","rapidjson"}
    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
        "SOL_ALL_SAFETIES_ON",
        "_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING" ,
    }
    postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/"}
    filter { "system:windows" }
        defines {"_WIN32_WINNT=0x0601"}
    filter {"system:linux"}
        links{"dl","pthread","stdc++fs"}
        --links{"stdc++:static"}
        --links{"gcc:static"}
        linkoptions {"-Wl,-rpath=./"}
    filter {"system:macosx"}
        links{"dl","pthread","c++fs"}
        linkoptions {"-Wl,-rpath,./"}
    filter "configurations:Debug"
        targetsuffix "-d"


--[[
    lua C/C++模块
    @dir： 模块源文件所在路径，相对于当前目录的路径
    @name: LUAMOD name
    @normaladdon : 平台通用的附加项
    @winddowsaddon : windows下的附加项
    @linuxaddon : linux下的附加项
    @macaddon : macosx下的附加项

    使用：
    模块编写规范：使用 LUAMOD_API 导出符号(windows)

    注意：
    默认使用C编译器编译，可以使用 *addon 参数进行更改
]]
local function add_lua_module(dir, name, normaladdon, winddowsaddon, linuxaddon, macaddon )
    project(name)
    objdir ("obj/"..name.."/%{cfg.platform}_%{cfg.buildcfg}") --编译生成的中间文件目录
    location ("build/"..name) -- 生成的工程文件目录
    kind "SharedLib" -- 静态库 StaticLib， 动态库 SharedLib
    targetdir "bin/%{cfg.buildcfg}" --目标文件目录
    includedirs {"./third","./third/lua53"} --头文件搜索目录
    files { dir.."/**.h",dir.."/**.hpp", dir.."/**.c",dir.."/**.cpp"} --需要编译的文件， **.c 递归搜索匹配的文件
    targetprefix "" -- linux 下需要去掉动态库 'lib' 前缀
    language "C"
    postbuildcommands{"{COPY} %{wks.location}/bin/%{cfg.buildcfg}/%{cfg.buildtarget.name} %{wks.location}/example/clib/"} -- 编译完后拷贝到example目录
    if type(normaladdon)=="function" then
        normaladdon()
    end
    filter { "system:windows" }
        links{"lua53"} -- windows 版需要链接 lua 库
        defines {"LUA_BUILD_AS_DLL","LUA_LIB"} -- windows下动态库导出宏定义
        if type(winddowsaddon)=="function" then
            winddowsaddon()
        end
    filter {"system:linux"}
        if type(linuxaddon)=="function" then
            linuxaddon()
        end
    filter {"system:macosx"}
        if type(macaddon)=="function" then
            macaddon()
        end
end

-----------------------------------------------------------------------------------
--[[
    Lua C/C++扩展 在下面添加
]]

-------------------------protobuf--------------------
add_lua_module("./third/protobuf", "protobuf",nil,
function()
    language "C++"
    buildoptions {"/TP"} -- protobuf库windows下需要强制用C++编译，默认会根据文件后缀名选择编译
end)

add_lua_module("./third/lcrypt", "crypt")

--[[
    lua版mysql,如果需要lua mysql 客户端，取消下面注释.
    依赖： 需要连接 mysql C client库,
    1. windows 下需要设置MYSQL_HOME.
    2. Linux 下需要确保mysql C client头文件目录和库文件目录正确
]]

-- ---------------------mysql-----------------------
-- add_lua_module("./lualib-src/mysql","mysql",
-- function()
--     language "C++"
-- end,
-- function ()
--     if os.istarget("windows") then
--         assert(os.getenv("MYSQL_HOME"),"please set mysql environment 'MYSQL_HOME'")
--         includedirs {os.getenv("MYSQL_HOME").. "/include"}
--         libdirs{os.getenv("MYSQL_HOME").. "/lib"} -- mysql C client库搜索目录
--         links{"libmysql"}
--     end
-- end,
-- function ()
--     if os.istarget("linux") then
--         assert(os.isdir("/usr/include/mysql"),"please make sure you have install mysql, or modify the default include path,'/usr/include/mysql'")
--         assert(os.isdir("/usr/lib64/mysql"),"please make sure you have install mysql, or modify the default lib path,'/usr/lib64/mysql'")
--         includedirs {"/usr/include/mysql"}
--         libdirs{"/usr/lib64/mysql"} -- mysql C client库搜索目录
--         links{"mysqlclient"}
--     end
-- end
-- )

-------------------------laoi--------------------
add_lua_module("./lualib-src/laoi", "aoi",function()
    language "C++"
end)