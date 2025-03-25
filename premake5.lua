---@diagnostic disable: undefined-global, undefined-field

if _ACTION ~= "build" and _ACTION ~= "clean" and _ACTION ~= "publish" then
    os.execute("git checkout master")
    os.execute("git pull")
    os.execute("git submodule init")
    os.execute("git submodule update")
end

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
        symbols "On"

    filter {"system:windows"}
        characterset "MBCS"
        systemversion "latest"
        warnings "Extra"
        cdialect "C11"
        buildoptions{"/experimental:c11atomics"}
        staticruntime "on"

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
        -- Export Lua library symbols, allowing them to be linked by extension libraries
        defines {"LUA_BUILD_AS_DLL"}
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
        "./src",
        "./src/moon",
        "./src/moon/core",
        "./third",
        "./third/lua",
        "./third/mimalloc/include"
    }

    files {
        "./src/moon/**.h",
        "./src/moon/**.hpp",
        "./src/moon/**.cpp"
    }

    links{
        "lua",
        "lualib",
        "crypt",
        "pb",
        "sharetable",
        "mongo",
        "mimalloc",
        "lfmt"
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
        includedirs {"./src", "./third","./third/lua"}
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
    "./third/lfmt",
    "lfmt"
)

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
    "./src/lualib-src",
    "lualib",
    {
        all = function()
            language "C++"
            includedirs {"./src/moon", "./src/moon/core", "./third/mimalloc/include"}
            defines {
                "ASIO_STANDALONE" ,
                "ASIO_NO_DEPRECATED",
                "MOON_ENABLE_MIMALLOC"
            }

            ---json
            ---files { "./third/yyjson/**.h", "./third/yyjson/**.c"}

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

local function cleanup()
    os.remove("moon")
    os.remove("moon.*")
    os.remove("lua.*")
    os.remove("liblua.*")
    os.rmdir("build/obj")
    os.rmdir("build/bin")
    os.rmdir(".vs")
end

-- if _ACTION == "clean" then
--     cleanup()
-- end

newaction {
    trigger = "clean",
    description = "Cleanup",
    execute = function ()
        cleanup()
    end
}


newaction {
    trigger = "build",
    description = "Build",
    execute = function ()
        local host = os.host()
        local switch = {
            windows = function ()
                os.execute("premake5.exe vs2022")
                local command = os.getenv("ProgramFiles(x86)")..[[\Microsoft Visual Studio\Installer\vswhere.exe]]
                command = string.format('"%s" %s', string_trim(command), " -latest -products * -requires Microsoft.Component.MSBuild -property installationPath")
                local handle = assert(io.popen(command))
                command = handle:read("*a")
                handle:close()
                os.execute(string.format('"%s%s" -maxcpucount:4 Server.sln /t:build /p:Configuration=Release ', string_trim(command), [[\MSBuild\Current\Bin\MSBuild.exe]]))
            end,
            linux = function ()
                os.execute("premake5 gmake2")
                os.execute("make -j4 config=release")
            end,
            macosx = function ()
                os.execute("premake5 gmake2 --cc=clang")
                os.execute("make -j4 config=release")
            end,
        }

        -- cleanup()
        switch[host]()
    end
}

newaction {
    trigger = "publish",
    description = "Publish",
    execute = function ()
        local host = os.host()
        local switch = {
            windows = function ()
                os.execute("if exist moon-windows.zip del /f moon-windows.zip")
                os.execute("if not exist clib mkdir clib")
                os.execute("echo Compressing files into moon-windows.zip...")
                os.execute("powershell Compress-Archive -Path moon.exe, lualib, service, clib, example, README.md -DestinationPath moon-windows.zip ")
                os.execute("echo Checking if moon-windows.zip was created...")
                os.execute("if exist moon-windows.zip (echo moon-windows.zip created successfully.) else (echo Failed to create moon-windows.zip.)")
            end,
            linux = function ()
                os.execute([[
                    #!/bin/bash
                    rm -f moon-linux.zip
                    mkdir -p clib
                    zip -r moon-linux.zip moon lualib service clib/*.so README.md example
                ]])
            end,
            macosx = function ()
                os.execute([[
                    #!/bin/bash
                    rm -f moon-macosx.zip
                    mkdir -p clib
                    zip -r moon-macosx.zip moon lualib service clib/*.dylib README.md example
                ]])
            end,
        }

        switch[host]()
    end
}
