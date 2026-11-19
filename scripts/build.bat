@echo off
setlocal
cd /d "%~dp0.."

set "VSDEV="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "C:\Program Files\Microsoft Visual Studio\18\%%E\Common7\Tools\VsDevCmd.bat" (
    set "VSDEV=C:\Program Files\Microsoft Visual Studio\18\%%E\Common7\Tools\VsDevCmd.bat"
  )
  if exist "C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat" (
    set "VSDEV=C:\Program Files\Microsoft Visual Studio\2022\%%E\Common7\Tools\VsDevCmd.bat"
  )
)
if not defined VSDEV (
  echo Visual Studio VsDevCmd.bat not found
  exit /b 1
)

set "CMAKE="
for %%E in (Community Professional Enterprise BuildTools) do (
  if exist "C:\Program Files\Microsoft Visual Studio\18\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE=C:\Program Files\Microsoft Visual Studio\18\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
)
if not defined CMAKE set "CMAKE=cmake"

if not exist build mkdir build
call "%VSDEV%" -arch=x64 -host_arch=x64
"%CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
"%CMAKE%" --build build
if errorlevel 1 exit /b 1
echo.
echo Built: build\bin\TaskbarEndHook.dll
echo        build\bin\TaskbarEndInjector.exe
endlocal
