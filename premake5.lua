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

     
project "protobuf"
    objdir "obj/protobuf/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/protobuf"
    kind "SharedLib"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./third/lua53","./third"}
    files { "./third/protobuf/**.h", "./third/protobuf/**.c"}
    links{"lua53"}
    targetprefix ""
    language "C++"
    filter { "system:windows" }
        defines {"LUA_BUILD_AS_DLL","_USRDLL"}
        buildoptions {"/TP"}

project "core"
    objdir "obj/core/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/core"
    kind "StaticLib"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./","./third","./core"}
    files { "./core/**.hpp","./core/**.h","./core/**.cpp"}
    language "C++"
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
    }
    filter { "system:windows" }
         defines {"_WIN32_WINNT=0x0601"}
    filter "configurations:Debug"
    	 targetsuffix "-d"

project "moon"
    objdir "obj/moon/%{cfg.platform}_%{cfg.buildcfg}"
    location "build/moon"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    includedirs {"./","./moon","./core","./third","./clib"}
    files {"./moon/**.h", "./moon/**.hpp","./moon/**.cpp" }
    links{"lua53","core","rapidjson"}
    defines {"SOL_CHECK_ARGUMENTS"}
    filter { "system:linux" }
        links{"dl","pthread"} --"-static-libstdc++"
        linkoptions {"-Wl,-rpath=./"}
    filter "configurations:Debug"
    	targetsuffix "-d"

