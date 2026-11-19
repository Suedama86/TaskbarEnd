@echo off
setlocal
cd /d "%~dp0.."

net session >nul 2>&1
if errorlevel 1 (
  echo Requesting admin...
  powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

if not exist "build\bin\TaskbarEndHook.dll" (
  call scripts\build.bat
  if errorlevel 1 exit /b 1
)

set "DEST=%LOCALAPPDATA%\TaskbarEnd"
mkdir "%DEST%" 2>nul
copy /Y "build\bin\TaskbarEndHook.dll" "%DEST%\" >nul
copy /Y "build\bin\TaskbarEndInjector.exe" "%DEST%\" >nul

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v TaskbarEnd /t REG_SZ /d "\"%DEST%\TaskbarEndInjector.exe\"" /f >nul

"%DEST%\TaskbarEndInjector.exe" /unload >nul 2>&1
timeout /t 1 /nobreak >nul
"%DEST%\TaskbarEndInjector.exe"
echo.
echo Installed to %DEST%
echo Right-click a taskbar app for End task.
endlocal
