
@echo off
SETLOCAL EnableDelayedExpansion

if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  echo "WARNING: You need VS 2017 version 15.2 or later (for vswhere.exe)"
)

set vswherestr=^"!ProgramFiles(x86)!\Microsoft Visual Studio\Installer\vswhere.exe^" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
for /f "usebackq tokens=*" %%i in (`!vswherestr!`) do (  
  set BUILDVCTOOLS=%%i\Common7\IDE
  echo BUILDVCTOOLS: !BUILDVCTOOLS!
  if not exist !BUILDVCTOOLS!\devenv.com (
    echo Error: Cannot find VS2017 or later Build Tools
    exit /b 2
  )
  git pull
  echo.!BUILDVCTOOLS! | findstr /C:"2022" >nul &&(
      "premake5.exe" "vs2022"
  ) || echo.!BUILDVCTOOLS! | findstr /C:"2019" >nul &&(
      "premake5.exe" "vs2019"
  ) || echo.!BUILDVCTOOLS! | findstr /C:"2017" >nul &&(
      "premake5.exe" "vs2017"
  ) || (
      echo Error: Cannot find VS2017 or later Build Tools
      exit /b 2
  )
  "!BUILDVCTOOLS!\devenv.com" "Server.sln" /Rebuild "Release"
)
