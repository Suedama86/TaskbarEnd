@echo off
setlocal
set "DEST=%LOCALAPPDATA%\TaskbarEnd"
if exist "%DEST%\TaskbarEndInjector.exe" (
  "%DEST%\TaskbarEndInjector.exe" /unload
)
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v TaskbarEnd /f >nul 2>&1
rmdir /s /q "%DEST%" 2>nul
echo TaskbarEnd removed.
endlocal
