---@diagnostic disable: undefined-global

local MOON_ENABLE_MIMALLOC = true

local LUA_BUILD_AS_DLL = true

workspace "Server"
    configurations { "Debug", "Release" }
    flags{"NoPCH","RelativeLinks"}
    cppdialect "C++17"
    location "./"
    architecture "x64"
    staticruntime "on"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        symbols "On"

    filter {"system:windows"}
        characterset "MBCS"
        systemversion "latest"
        warnings "Extra"
        cdialect "C11"
        buildoptions{"/experimental:c11atomics"}
        if LUA_BUILD_AS_DLL then
            staticruntime "off"
        end

    filter { "system:linux" }
        warnings "High"

    filter { "system:macosx" }
        warnings "High"

if LUA_BUILD_AS_DLL and os.host() == "windows" then
    project "lua"
        location "build/projects/%{prj.name}"
        objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
        targetdir "build/bin/%{cfg.buildcfg}"
        kind "SharedLib"
        language "C"
        includedirs {"./third/lua"}
        files {"./third/lua/onelua.c"}
        defines {"MAKE_LIB"}
        filter { "system:windows" }
            defines {"LUA_BUILD_AS_DLL"}
            disablewarnings { "4244","4324","4702","4310", "4701"}
            filter{"configurations:*"}
                postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}
else
    project "lua"
        location "build/projects/%{prj.name}"
        objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
        targetdir "build/bin/%{cfg.buildcfg}"
        kind "StaticLib"
        language "C"
        includedirs {"./third/lua"}
        files {"./third/lua/onelua.c"}
        defines {"MAKE_LIB"}
        filter { "system:windows" }
            disablewarnings { "4244","4324","4702","4310", "4701"}
        filter { "system:linux" }
            defines {"LUA_USE_LINUX"}
        filter { "system:macosx" }
            defines {"LUA_USE_MACOSX"}
end

if MOON_ENABLE_MIMALLOC then
    os.execute("git submodule init")
    os.execute("git submodule update")

    project "mimalloc"
        location "build/projects/%{prj.name}"
        objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
        targetdir "build/bin/%{cfg.buildcfg}"
        kind "StaticLib"
        language "C"
        includedirs {"./third/mimalloc/include"}
        files {"./third/mimalloc/src/static.c"}
end

project "moon"
    location "build/projects/%{prj.name}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.buildcfg}"

    kind "ConsoleApp"
    language "C++"
    includedirs {"./","./moon-src","./moon-src/core", "./third", "./third/lua"}
    files {"./moon-src/**.h", "./moon-src/**.hpp","./moon-src/**.cpp" }
    links{
        "lua",
        "lualib",
        "crypt",
        "pb",
        "sharetable",
        "mongo",
    }
    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
    }

    if MOON_ENABLE_MIMALLOC then
        links{"mimalloc"}
        defines {"MOON_ENABLE_MIMALLOC"}
        includedirs {"./third/mimalloc/include"}
    end

    filter { "system:windows" }
        defines {"_WIN32_WINNT=0x0601"}
        linkoptions { '/STACK:"8388608"' }
    filter {"system:linux"}
        links{"dl","pthread","stdc++fs"}
        linkoptions {"-static-libstdc++ -static-libgcc", "-Wl,-E,--as-needed,-rpath=./"}
    filter {"system:macosx"}
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
local function add_lua_module(dir, name, normaladdon, windowsaddon, linuxaddon, macaddon )
    project(name)
        location("build/projects/%{prj.name}")
        objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
        targetdir "build/bin/%{cfg.buildcfg}"

        language "C"
        kind "StaticLib"
        includedirs {"./", "./third","./third/lua"}
        files { dir.."/**.h",dir.."/**.hpp", dir.."/**.c",dir.."/**.cpp"}

        defines{"SOL_ALL_SAFETIES_ON"}

        if type(normaladdon)=="function" then
            normaladdon()
        end

        filter { "system:windows" }
            if type(windowsaddon)=="function" then
                windowsaddon()
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

----------------------Lua C/C++ Modules------------------------

add_lua_module("./third/sharetable", "sharetable")
add_lua_module("./third/lcrypt", "crypt", nil, function ()
    disablewarnings { "4244","4267","4456", "4459"}
end)
add_lua_module("./third/pb", "pb")--protobuf
add_lua_module("./third/lmongo", "mongo",nil, function ()
    disablewarnings { "4267","4457","4456", "4459", "4996", "4244", "4310"}
end)

add_lua_module("./lualib-src", "lualib", function()
    language "C++"
    includedirs {"./moon-src", "./moon-src/core"}
    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
    }

    if MOON_ENABLE_MIMALLOC then
        includedirs {"./third/mimalloc/include"}
        defines {"MOON_ENABLE_MIMALLOC"}
    end

    ---json
    defines{ "YYJSON_DISABLE_WRITER" }
    files { "./third/yyjson/**.h", "./third/yyjson/**.c"}

    ---kcp
    files { "./third/kcp/**.h", "./third/kcp/**.c"}

    ---navmesh begin
    includedirs {
        "./third/recastnavigation/Detour/Include",
        "./third/recastnavigation/DetourCrowd/Include",
        "./third/recastnavigation/DetourTileCache/Include",
        "./third/recastnavigation/Recast/Include"
    }

    files {
        "./third/recastnavigation/Detour/**.h",
        "./third/recastnavigation/Detour/**.cpp",
        "./third/recastnavigation/DetourCrowd/**.h",
        "./third/recastnavigation/DetourCrowd/**.cpp",
        "./third/recastnavigation/DetourTileCache/**.h",
        "./third/recastnavigation/DetourTileCache/**.cpp",
        "./third/recastnavigation/Recast/**.h",
        "./third/recastnavigation/Recast/**.cpp",
        "./third/fastlz/**.h",
        "./third/fastlz/**.c"
    }
    ---navmesh end
end, function ()
    defines {"_WIN32_WINNT=0x0601"}
end)
