
@echo off
SETLOCAL EnableDelayedExpansion

if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  echo "WARNING: You need VS 2022 (for vswhere.exe)"
)

set vswherestr=^"!ProgramFiles(x86)!\Microsoft Visual Studio\Installer\vswhere.exe^" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
for /f "usebackq tokens=*" %%i in (`!vswherestr!`) do (  
  set BUILDVCTOOLS=%%i\Common7\IDE
  echo BUILDVCTOOLS: !BUILDVCTOOLS!
  if not exist !BUILDVCTOOLS!\devenv.com (
    echo Error: Cannot find VS2022
    exit /b 2
  )
  git pull
  echo.!BUILDVCTOOLS! | findstr /C:"2022" >nul &&(
      "premake5.exe" "vs2022"
  ) || (
      echo Error: Cannot find vs2022
      exit /b 2
  )
  "!BUILDVCTOOLS!\devenv.com" "Server.sln" /Rebuild "Release"
)
