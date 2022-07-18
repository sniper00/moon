---@diagnostic disable: undefined-global

workspace "Proxy"
    configurations { "Debug", "Release" }
    flags{"NoPCH","RelativeLinks"}
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

    filter { "system:linux" }
        warnings "High"

    filter { "system:macosx" }
        warnings "High"

project "proxy"
    location "build/projects/%{prj.name}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.buildcfg}"

    kind "ConsoleApp"
    language "C++"
    includedirs {"./", "../third"}
    files {"./**.h", "./**.hpp","./**.cpp" }

    defines {
        "ASIO_STANDALONE" ,
        "ASIO_NO_DEPRECATED",
    }

    filter { "system:windows" }
        cppdialect "C++17"
        defines {"_WIN32_WINNT=0x0601"}
        buildoptions { "/await" }
    filter {"system:linux"}
        buildoptions {"-std=c++20"}
        links{"pthread"}
        linkoptions {"-static-libstdc++ -static-libgcc", "-Wl,-rpath=./","-Wl,--as-needed"}
    filter {"system:macosx"}
        buildoptions {"-std=c++20"}
        links{"pthread"}
        linkoptions {"-Wl,-rpath,./"}
    filter "configurations:Debug"
        targetsuffix "-d"
    filter{"configurations:*"}
        postbuildcommands{"{COPY} %{cfg.buildtarget.abspath} %{wks.location}"}