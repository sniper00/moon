---@diagnostic disable: undefined-global, undefined-field

-- Global command-line option to specify release configuration
newoption {
   trigger     = "release",
   description = "Generate/Build release configuration"
}

newoption {
  trigger = "package",
  value = "URL",
  description = "Add an external package from a Git repository"
}

-- Helper function to trim whitespace (or specified characters) from a string
local function string_trim(input, chars)
    chars = chars or " \t\n\r"
    local pattern = "^[" .. chars .. "]+"
    input = string.gsub(input, pattern, "")
    pattern = "[" .. chars .. "]+$"
    return string.gsub(input, pattern, "")
end

---------------------- Helper Functions for Actions ------------------------

-- Determines the target configuration based on the --release flag.
local function get_build_config()
    local cfg_name = "debug"
    if _OPTIONS.release then
        cfg_name = "release"
    end
    print("Using configuration: " .. cfg_name)
    return cfg_name
end

-- Generates the target files (Makefiles, VS solutions, etc.) for the host OS.
local function generate_build_files(host)
    print("Ensuring target files are generated...")
    local generate_commands = {
        windows = "premake5.exe vs2022",
        linux   = "premake5 gmake",
        macosx  = "premake5 gmake --cc=clang"
    }
    local gen_cmd = generate_commands[host]
    if not gen_cmd then error("Unsupported host OS for generation: " .. host) end
    local _, _, gen_cmd_code = os.execute(gen_cmd)
    if gen_cmd_code ~= 0 then error("Generation failed! code: ".. gen_cmd_code) end
    print("Generation complete.")
end

-- Compiles the project for the specified host OS and configuration.
local function compile_project(host, cfg_name)
    print("Ensuring project is built...")
    local compile_commands = {
        windows = function()
            -- Find MSBuild path dynamically
            local msbuild_path_cmd = os.getenv("ProgramFiles(x86)")..[[\Microsoft Visual Studio\Installer\vswhere.exe]]
            msbuild_path_cmd = string.format('"%s" %s', string_trim(msbuild_path_cmd), " -latest -products * -requires Microsoft.Component.MSBuild -property installationPath")
            local handle = assert(io.popen(msbuild_path_cmd))
            local install_path = handle:read("*a")
            handle:close()
            if not install_path or string.len(string_trim(install_path)) == 0 then
                error("Could not find MSBuild installation path.")
            end
            local msbuild_exe = string.format('"%s%s"', string_trim(install_path), [[\MSBuild\Current\Bin\MSBuild.exe]])
            -- Return the full MSBuild command
            return string.format('%s -maxcpucount:4 target/Server.sln /t:build /p:Configuration=%s ', msbuild_exe, cfg_name)
        end,
        linux   = string.format("make -C target -j4 config=%s", string.lower(cfg_name)),
        macosx  = string.format("make -C target -j4 config=%s", string.lower(cfg_name))
    }
    local compile_cmd = compile_commands[host]
    if not compile_cmd then error("Unsupported host OS for compilation: " .. host) end
    if type(compile_cmd) == "function" then
        compile_cmd = compile_cmd() -- Execute function for Windows
    end
    local _, _, compile_code = os.execute(compile_cmd)
    if compile_code ~= 0 then error("Compilation failed! code: ".. compile_code) end
    print("Build check/compilation complete.")
end

function get_sharedlib_name(name)
    if os.host() == "windows" and name:sub(1,3) == "lib" then
        name = name:sub(4)
    end
    local host = os.host()
    local ext_map = {
        windows = "dll",
        linux   = "so",
        macosx  = "dylib"
    }
    return name .. "." .. (ext_map[host] or "so")
end

---------------------- Custom Actions ------------------------

-- Custom action: clean
-- Removes target outputs and intermediate files.
newaction {
    trigger = "clean",
    description = "Remove target artifacts",
    execute = function ()
        os.rmdir("target")
        local dirs = os.matchdirs("ext/**/target")
        for _, dir in ipairs(dirs) do
            print("Removing directory: " .. dir)
            os.rmdir(dir)
        end
        print("Clean complete.")
    end
}

-- Custom action: add
-- Clones a Git repository into the 'ext' directory.
-- Requires the --url option to specify the repository URL.
newaction {
    trigger = "add",
    description = "Add an external package from a Git repository",
    execute = function (...)
        local package = _OPTIONS.package
        if not package or package == "" then
            print("Error: --package option is required to add a package.")
            return
        end

        if os.isdir("ext/" .. package:match("/([^/]+)%.git$")) then
            print("Package already exists: " .. package)
            return
        end

        print("Adding external package from git repository...", package)
        os.execute("git clone " .. package .. " ext/" .. package:match("/([^/]+)%.git$"))
    end
}

-- Custom action: add
-- Clones a Git repository into the 'ext' directory.
-- Requires the --url option to specify the repository URL.
newaction {
    trigger = "remove",
    description = "Remove an external package",
    execute = function (...)
        local package = _OPTIONS.package
        if not package or package == "" then
            print("Error: --package option is required to remove a package.")
            return
        end

        if not os.isdir("ext/" .. package:match("/([^/]+)%.git$")) then
            print("Package does not exist: " .. package)
            return
        end

        print("Removing external package...", package)
        os.execute("rm -rf ext/" .. package:match("/([^/]+)%.git$"))
    end
}

-- Custom action: target
-- Generates target files (if needed) and compiles the project.
-- Uses the global --release option to determine configuration.
newaction {
    trigger = "build",
    description = "Generate and build the project (default: debug, use --release for release)",
    execute = function ()
        local cfg_name = get_build_config()
        local host = os.host()

        -- Optional: Update sources before building
        print("Updating git submodules...")
        os.execute("git checkout master")
        os.execute("git pull")
        os.execute("git submodule init")
        os.execute("git submodule update")

        generate_build_files(host)
        compile_project(host, cfg_name)

        print("Build completed successfully for configuration: " .. cfg_name)
    end
}

-- Custom action: publish
-- Creates distributable archives for different platforms.
newaction {
    trigger = "publish",
    description = "Create distributable archives",
    execute = function ()
        local host = os.host()
        local switch = {
            windows = function ()
                os.execute("if exist moon-windows.zip del /f moon-windows.zip")
                os.execute("if not exist clib mkdir clib")
                os.copyfile("target/release/moon.exe", "moon.exe")
                os.execute("echo Compressing files into moon-windows.zip...")
                os.execute("powershell Compress-Archive -Path moon.exe, lualib, service, clib, example, README.md -DestinationPath moon-windows.zip ")
                os.execute("echo Checking if moon-windows.zip was created...")
                os.execute("if exist moon-windows.zip (echo moon-windows.zip created successfully.) else (echo Failed to create moon-windows.zip.)")
                os.remove("moon.exe")
            end,
            linux = function ()
                os.copyfile("target/release/moon", "moon")
                os.execute([[
                    #!/bin/bash
                    rm -f moon-linux.zip
                    mkdir -p clib
                    zip -r moon-linux.zip moon lualib service clib/*.so README.md example
                ]])
                os.remove("moon")
            end,
            macosx = function ()
                os.copyfile("target/release/moon", "moon")
                os.execute([[
                    #!/bin/bash
                    rm -f moon-macosx.zip
                    mkdir -p clib
                    zip -r moon-macosx.zip moon lualib service clib/*.dylib README.md example
                ]])
                os.remove("moon")
            end,
        }

        -- Execute the packaging command for the host OS
        if switch[host] then
            switch[host]()
        else
            error("Unsupported host OS for publishing: " .. host)
        end
        print("Publishing process completed.")
    end
}

-- Custom action: run
-- Generates target files, builds (if necessary), and runs the project.
-- Uses the global --release option to determine configuration.
-- Passes any additional arguments to the executable.
newaction {
    trigger     = "run",
    description = "Build and run the project (default: debug, use --release for release)",
    execute = function ()
        -- 1. Determine config, generate target files, and compile
        local cfg_name = get_build_config()
        local host = os.host()
        generate_build_files(host)
        compile_project(host, cfg_name)

        -- 2. Determine executable path
        local executable_path
        local target_dir = path.join("target", cfg_name)
        if host == "windows" then
            executable_path = path.join(target_dir, "moon.exe")
            executable_path = path.translate(executable_path)
        else
            executable_path = path.join(target_dir, "moon")
        end

        -- 3. Collect arguments passed after 'run'
        local run_args_list = {}
        for _, arg in ipairs(_ARGS) do
            -- Quote arguments containing spaces for robustness
            if string.find(arg, " ", 1, true) then
                 table.insert(run_args_list, '"' .. arg .. '"')
            else
                 table.insert(run_args_list, arg)
            end
        end
        local run_args_str = table.concat(run_args_list, " ")

        -- 4. Construct and execute the run command
        local final_executable_path = executable_path
        -- Quote executable path if it contains spaces
        if string.find(executable_path, " ", 1, true) then
             final_executable_path = '"' .. executable_path .. '"'
        end

        local run_command = final_executable_path .. " " .. run_args_str
        print("Running: " .. run_command)
        local _, _, run_code = os.execute(run_command)
        if run_code ~= 0 then
             print("Executable finished with non-zero exit code: " .. run_code)
        else
             print("Executable finished successfully.")
        end
    end
}

-- Workspace definition: overarching settings for the solution/projects
workspace "Server"
    -- Define available configurations
    configurations { "debug", "release" }
    -- Common flags for all projects
    flags{"NoPCH","RelativeLinks"}
    -- C++ standard
    cppdialect "C++17"
    -- Output location for generated project files (e.g., .sln, Makefile)
    location "./target"
    -- Target architecture
    architecture "x64"

    -- Configuration-specific settings
    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"

    -- Configuration-specific settings
    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"
        symbols "On"

    -- Platform-specific settings
    filter {"system:windows"}
        characterset "MBCS"
        systemversion "latest"
        warnings "Extra"
        cdialect "C11"
        buildoptions{"/experimental:c11atomics"}
        staticruntime "on"


    -- Platform-specific settings
    filter { "system:linux" }
        warnings "High"

    -- Platform-specific settings
    filter { "system:macosx" }
        warnings "High"

-- Base Lua library project
project "lua"
    location "target/projects/%{prj.name}"
    objdir "target/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "target/%{cfg.buildcfg}"
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

-- Memory allocator project
project "mimalloc"
    location "target/projects/%{prj.name}"
    objdir "target/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "target/%{cfg.buildcfg}"
    kind "StaticLib"
    language "C"
    includedirs {"./third/mimalloc/include"}
    files {"./third/mimalloc/src/static.c"}


-- Main application project
project "moon"
    location "target/projects/%{prj.name}"
    objdir "target/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "target/%{cfg.buildcfg}"

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

--
-- Remember moon location; Will need it to locate sub-scripts later.
--
MOON_DIR = _SCRIPT_DIR


--- Adds a Lua module to the target configuration.
---
--- This function configures a Lua module located in the specified directory
--- and assigns it the given name. Additional platform-specific options can
--- be provided to customize the target process for different operating systems.
---
--- @param dir string The path to the module's source files, relative to the current directory.
--- @param name string The name of the Lua module.
--- @param options? table Additional options for the module configuration.
---                      The options table can contain the following keys:
---                      - all: function() end - A function to be executed for all platforms.
---                      - windows: function() end - A function to be executed only on Windows.
---                      - linux: function() end - A function to be executed only on Linux.
---                      - macosx: function() end - A function to be executed only on macOS.
--- @param is_shared? boolean Whether to target the module as a shared library (true) or static library (false). Default is false (static).
function add_lua_module(dir, name, options, is_shared)     
    project(name)
       if not options then
            options = {}
        end

        location(MOON_DIR.."/target/projects/%{prj.name}")
        objdir(MOON_DIR.."/target/obj/%{prj.name}/%{cfg.buildcfg}")
        targetdir(MOON_DIR.."/target/%{cfg.buildcfg}")
        language "C"
        if is_shared then
            kind "SharedLib"
        else
            kind "StaticLib"
        end
    
        includedirs {MOON_DIR .. "/src", MOON_DIR.. "/third", MOON_DIR.."/third/lua"}
        files { dir.."/**.h",dir.."/**.hpp", dir.."/**.c",dir.."/**.cpp"}

        defines{"SOL_ALL_SAFETIES_ON"}

 
        if type(options.all)=="function" then
            options.all()
        end

        filter { "system:windows" }
            if is_shared then
                defines{"LUA_BUILD_AS_DLL"}
                links{"moon"}
            end
            if type(options.windows)=="function" then
                options.windows()
            end
        filter {"system:linux"}
            if is_shared then
                targetprefix ""
            end
            if type(options.linux)=="function" then
                options.linux()
            end
        filter {"system:macosx"}
            if is_shared then
                targetprefix ""
                linkoptions {
                    "-undefined dynamic_lookup"
                }
            end

            if type(options.macosx)=="function" then
                options.macosx()
            end
        if is_shared then
            filter{"configurations:*"}
                postbuildcommands{"{COPY} %{cfg.buildtarget.relpath} "..MOON_DIR.."/clib/"}
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

local files = os.matchfiles("ext/**/premake5.lua")
if files ~= nil and #files > 0 then
    os.mkdir("clib")
    os.mkdir("lualib/ext")
end
for _, file in ipairs(files) do
    
    --print("Including external premake file: " .. file)
    include(file)
end
