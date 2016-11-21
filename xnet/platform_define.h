/****************************************************************************

Git <https://github.com/sniper00/MoonNetLua>
E-Mail <hanyongtao@live.com>
Copyright (c) 2015-2016 moon
Licensed under the MIT License <http://opensource.org/licenses/MIT>.

****************************************************************************/

#pragma once

#define PLATFORM_UNKNOWN			0
#define PLATFORM_WINDOWS			1
#define PLATFORM_LINUX					2
#define PLATFORM_MAC					3

#define TARGET_PLATFORM				PLATFORM_UNKNOWN

// mac
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__xlC__) || defined(__xlc__))  
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_MAC
#endif

// win32
#if !defined(SAG_COM) && (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))  
#if defined(WINCE) || defined(_WIN32_WCE)
//win ce
#else
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_WINDOWS
#endif
#endif

#if !defined(SAG_COM) && (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_WINDOWS
#endif

// linux
#if defined(__linux__) || defined(__linux) 
#undef  TARGET_PLATFORM
#define TARGET_PLATFORM         PLATFORM_LINUX
#endif

//////////////////////////////////////////////////////////////////////////
// post configure
//////////////////////////////////////////////////////////////////////////

// check user set platform
#if ! TARGET_PLATFORM
#error  "Cannot recognize the target platform; are you targeting an unsupported platform?"
#endif 

#if (TARGET_PLATFORM == PLATFORM_WINDOWS)
#ifndef __MINGW32__
#pragma warning (disable:4127) 
#endif 
#endif  // PLATFORM_WIN32
