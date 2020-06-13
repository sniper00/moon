workspace "Server"
    configurations { "Debug", "Release" }
    flags{"NoPCH","RelativeLinks"}
    cppdialect "C++17"
    location "./"
    architecture "x64"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        --symbols "On" --带上调试信息

    filter {"system:windows"}
        characterset "MBCS"
        systemversion "latest"
        warnings "Extra"

    filter { "system:linux" }
        warnings "High"

    filter { "system:macosx" }
        warnings "High"

project "lua53"
    location "projects/build/lua53"
    objdir "projects/obj/%{cfg.project.name}/%{cfg.platform}_%{cfg.buildcfg}"
    targetdir "projects/bin/%{cfg.buildcfg}"
    kind "StaticLib"
    language "C"
    includedirs {"./third/lua53"}
    files { "./third/lua53/**.h", "./third/lua53/**.c"}
    removefiles("./third/lua53/luac.c")
    removefiles("./third/lua53/lua.c")
    filter { "system:windows" }
        disablewarnings { "4244","4324","4702","4310" }
        -- defines {"LUA_BUILD_AS_DLL"}
    filter { "system:linux" }
        defines {"LUA_USE_LINUX"}
        -- links{"dl"}
    filter { "system:macosx" }
        defines {"LUA_USE_MACOSX"}
        -- links{"dl"}
    -- filter{"configurations:*"}
    --     postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}

project "moon"
    location "projects/build/moon"
    objdir "projects/obj/%{cfg.project.name}/%{cfg.platform}_%{cfg.buildcfg}"
    targetdir "projects/bin/%{cfg.buildcfg}"

    kind "ConsoleApp"
    language "C++"
    includedirs {"./","./moon-src","./moon-src/core","./third","./third/lua53"}
    files {"./moon-src/**.h", "./moon-src/**.hpp","./moon-src/**.cpp" }
    links{
        "lua53",
        "aoi",
        "crypt",
        "pb",
        "sharetable",
        "clonefunc"
    }
    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
        "SOL_ALL_SAFETIES_ON",
        "_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING"
    }
    filter { "system:windows" }
        defines {"_WIN32_WINNT=0x0601"}
    filter {"system:linux"}
        links{"dl","pthread","stdc++fs"}
        linkoptions {"-static-libstdc++ -static-libgcc", "-Wl,-rpath=./","-Wl,--as-needed"}
    filter {"system:macosx"}
        if os.istarget("macosx") then
            local tb = os.matchfiles("/usr/local/Cellar/llvm/**/c++fs.a")
            if #tb > 0 then
                print("use c++fs.a: ", tb[1])
                libdirs({path.getdirectory(tb[1])})
                links{"c++fs"}
            end
        end
        links{"dl","pthread"}
        linkoptions {"-Wl,-rpath,./"}
    filter "configurations:Debug"
        targetsuffix "-d"
    filter{"configurations:*"}
        postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}


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
        location("projects/build/"..name)
        objdir "projects/obj/%{cfg.project.name}/%{cfg.platform}_%{cfg.buildcfg}"--编译生成的中间文件目录
        targetdir "projects/bin/%{cfg.buildcfg}"--目标文件目录

        kind "StaticLib" -- 静态库 StaticLib， 动态库 SharedLib
        includedirs {"./", "./third","./third/lua53"} --头文件搜索目录
        files { dir.."/**.h",dir.."/**.hpp", dir.."/**.c",dir.."/**.cpp"} --需要编译的文件， **.c 递归搜索匹配的文件
        --targetprefix "" -- linux 下需要去掉动态库 'lib' 前缀
        language "C"
        defines{"SOL_ALL_SAFETIES_ON"}

        if type(normaladdon)=="function" then
            normaladdon()
        end
        filter { "system:windows" }
            --links{"lua53"} -- windows 版需要链接 lua 库
            --defines {"LUA_BUILD_AS_DLL","LUA_LIB"} -- windows下动态库导出宏定义
            if type(winddowsaddon)=="function" then
                winddowsaddon()
            end
        filter {"system:linux"}
            if type(linuxaddon)=="function" then
                linuxaddon()
            end
        filter {"system:macosx"}
            -- links{"lua53"}
            if type(macaddon)=="function" then
                macaddon()
            end
        --filter{"configurations:*"}
            --postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}/clib"}
end

-----------------------------------------------------------------------------------
--[[
    Lua C/C++扩展 在下面添加
]]

-------------------------protobuf--------------------
add_lua_module("./third/pb", "pb")

add_lua_module("./third/lcrypt", "crypt",nil,function ()
    disablewarnings { "4267","4456","4459","4244" }
end)

-------------------------laoi--------------------
add_lua_module("./lualib-src/laoi", "aoi",function()
    language "C++"
end)

-------------------------sharetable--------------------
add_lua_module("./third/sharetable", "sharetable")

-------------------------clonefunc: for hotfix--------------------
add_lua_module("./third/lclonefunc", "clonefunc")

--include("./lualib-src/mysql/premake5.lua")