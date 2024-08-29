---@diagnostic disable: undefined-global

os.execute("git pull")
os.execute("git submodule init")
os.execute("git submodule update")

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
        if LUA_BUILD_AS_DLL then
            kind "SharedLib"
            defines {"LUA_BUILD_AS_DLL"}
            postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}
        end
    filter { "system:linux" }
        defines {"LUA_USE_LINUX"}
    filter { "system:macosx" }
        defines {"LUA_USE_MACOSX"}


project "mimalloc"
    location "build/projects/%{prj.name}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.buildcfg}"
    kind "StaticLib"
    language "C"
    includedirs {"./third/mimalloc/include"}
    files {"./third/mimalloc/src/static.c"}


project "moon"
    location "build/projects/%{prj.name}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.buildcfg}"

    kind "ConsoleApp"
    language "C++"
    includedirs {
        "./",
        "./moon-src",
        "./moon-src/core",
        "./third",
        "./third/lua",
        "./third/mimalloc/include"
    }

    files {
        "./moon-src/**.h",
        "./moon-src/**.hpp",
        "./moon-src/**.cpp"
    }

    links{
        "lua",
        "lualib",
        "crypt",
        "pb",
        "sharetable",
        "mongo",
        "mimalloc"
    }

    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
        "MOON_ENABLE_MIMALLOC"
    }

    filter { "system:windows" }
        defines {"_WIN32_WINNT=0x0601"}
        linkoptions { '/STACK:"8388608"' }
    filter {"system:linux"}
        links{"dl","pthread","stdc++fs"}
        linkoptions {
            "-static-libstdc++ -static-libgcc",
            "-Wl,-E,--as-needed,-rpath=./"
        }
    filter {"system:macosx"}
        links{"dl","pthread"}
        linkoptions {
            "-Wl,-rpath,./",
            "-undefined dynamic_lookup"
        }
    filter "configurations:Debug"
        targetsuffix "-d"
    filter{"configurations:*"}
        postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}


--- Adds a Lua module to the build configuration.
---
--- This function configures a Lua module located in the specified directory
--- and assigns it the given name. Additional platform-specific options can
--- be provided to customize the build process for different operating systems.
---
--- @param dir string The path to the module's source files, relative to the current directory.
--- @param name string The name of the Lua module.
--- @param options? table Additional options for the module configuration.
---                      The options table can contain the following keys:
---                      - all: function() end - A function to be executed for all platforms.
---                      - windows: function() end - A function to be executed only on Windows.
---                      - linux: function() end - A function to be executed only on Linux.
---                      - macosx: function() end - A function to be executed only on macOS.
local function add_lua_module(dir, name, options )
    project(name)
        location("build/projects/%{prj.name}")
        objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
        targetdir "build/bin/%{cfg.buildcfg}"

        language "C"
        kind "StaticLib"
        includedirs {"./", "./third","./third/lua"}
        files { dir.."/**.h",dir.."/**.hpp", dir.."/**.c",dir.."/**.cpp"}

        defines{"SOL_ALL_SAFETIES_ON"}

        if not options then
            options = {}
        end

        if type(options.all)=="function" then
            options.all()
        end

        filter { "system:windows" }
            if type(options.windows)=="function" then
                options.windows()
            end
        filter {"system:linux"}
            if type(options.linux)=="function" then
                options.linux()
            end
        filter {"system:macosx"}
            if type(options.macosx)=="function" then
                options.macosx()
            end
end

----------------------Lua C/C++ Modules------------------------

add_lua_module(
    "./third/sharetable",
    "sharetable"
)

add_lua_module(
    "./third/lcrypt",
    "crypt",
    {
        windows = function ()
            disablewarnings { "4244","4267","4456", "4459"}
        end
    }
)

add_lua_module(
    "./third/pb",
    "pb"
)--protobuf

add_lua_module(
    "./third/lmongo",
    "mongo",
    {
        windows = function ()
            disablewarnings { "4267","4457","4456", "4459", "4996", "4244", "4310"}
        end
    }
)

add_lua_module(
    "./lualib-src",
    "lualib",
    {
        all = function()
            language "C++"
            includedirs {"./moon-src", "./moon-src/core", "./third/mimalloc/include"}
            defines {
                "ASIO_STANDALONE" ,
                "ASIO_NO_DEPRECATED",
                "MOON_ENABLE_MIMALLOC"
            }

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
        end,
        windows = function ()
            defines {"_WIN32_WINNT=0x0601"}
        end
    }
)

local function string_trim(input, chars)
    chars = chars or " \t\n\r"
    local pattern = "^[" .. chars .. "]+"
    input = string.gsub(input, pattern, "")
    pattern = "[" .. chars .. "]+$"
    return string.gsub(input, pattern, "")
end

newaction {
    trigger = "build",
    description = "Creates source and binary packages",
    execute = function ()
        local host = os.host()
        local switch = {
            windows = function ()
                os.execute("premake5.exe clean")
                os.execute("premake5.exe vs2022")
                local vswhere = os.getenv("programfiles")..[[ (x86)\Microsoft Visual Studio\Installer\vswhere.exe]]
                local command = string.format('"%s" %s', vswhere, " -latest -products * -requires Microsoft.Component.MSBuild -property installationPath")
                local handle = assert(io.popen(command))
                local result = handle:read("*a")
                handle:close()
                result = string_trim(result)
                os.execute(string.format('"%s%s" -maxcpucount:4 Server.sln /t:rebuild /p:Configuration=Release ', result, [[\MSBuild\Current\Bin\MSBuild.exe]]))
            end,
            linux = function ()
                os.execute("premake5 clean")
                os.execute("premake5 gmake2")
                os.execute("make -j4 config=release")
            end,
            macosx = function ()
                os.execute("premake5 clean")
                os.execute("premake5 gmake2 --cc=clang")
                os.execute("make -j4 config=release")
            end,
        }

        switch[host]()
    end
}

if _ACTION == "clean" then
    os.rmdir("build/obj")
    os.rmdir("build/bin")
    os.rmdir(".vs")
end
