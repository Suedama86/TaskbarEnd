# TaskbarEnd For Windows 10

Right-click a running app on the Windows 10 taskbar and end it, the same idea as Windows 11’s built-in **End task**.

A small DLL inside `explorer.exe` shows a red **End task** / **Avsluta aktivitet** button next to the icon. Click it and that app is closed. Click anywhere else, or press Esc, and the button goes away.

This is an independent project. It is not a fork of any other taskbar utility.

## Features

- End the app under a taskbar icon from a right-click
- Label follows the Windows language (`Avsluta aktivitet` on Swedish, `End task` otherwise)
- **Esc** or a left-click outside the button cancels
- Starts with Windows after setup
- No tray icon and no window of its own

## Requirements

- Windows 10, 64-bit. It also runs on Windows 11, next to the built-in command.
- A release zip needs nothing else. Building from source needs Visual Studio 2022 or 2026 with the Desktop development with C++ workload.

## Download

The ready-to-run build is on [Releases](https://github.com/Suedama86/TaskbarEnd/releases). Unzip it and run `setup.bat`. That copies TaskbarEnd to `%LOCALAPPDATA%\TaskbarEnd\`, starts it with Windows, and loads it into the current `explorer.exe`.

## Install from source

```bat
scripts\setup.bat
```

That script asks for administrator once, builds if needed, then installs the same way as the release zip.

## Use

| Action | What happens |
| --- | --- |
| Right-click a taskbar app | The red button appears above the icon |
| Click **End task** | That app is closed |
| Left-click anywhere else | The button is dismissed |
| Esc | The button is dismissed |

The button disappears on its own after a few seconds if you do nothing.

## Uninstall

```bat
scripts\uninstall.bat
```

That unloads the hook, removes the autostart entry, and deletes `%LOCALAPPDATA%\TaskbarEnd\`.

## Build from source

You need Visual Studio 2022 or 2026 (Community is enough) with the **Desktop development with C++** workload. CMake and Ninja come with that install. `scripts\build.bat` finds them.

```bat
scripts\build.bat
```

Output:

```
build\bin\TaskbarEndHook.dll
build\bin\TaskbarEndInjector.exe
```

Load or unload without the installer:

```bat
build\bin\TaskbarEndInjector.exe
build\bin\TaskbarEndInjector.exe /unload
```

A log is written to `%TEMP%\TaskbarEnd.log`.

## How it works

`TaskbarEndInjector` loads `TaskbarEndHook.dll` into the `explorer.exe` process that owns the taskbar. The hook watches for a right-click on a taskbar button, resolves which app that icon belongs to, and shows the End task button. Choosing the button closes that app’s windows and terminates its process.

Work that talks to UI Automation stays on a private thread. The taskbar’s own message hook only records the click and returns. That split is what keeps Explorer from restarting.

### Rules that keep the taskbar alive

1. The `WH_GETMESSAGE` hook stays tiny. No UI Automation, COM, or MSAA inside it.
2. There is no mouse hook inside Explorer, and no hook on `TrackPopupMenu`. The End task control is a separate button, not a menu item.
3. A hover target is only reused for the same icon. Moving to another icon drops it.
4. Universal Windows apps are ended by their real process (for example `SystemSettings.exe`), not by killing `ApplicationFrameHost.exe`.
5. `explorer.exe` is never terminated. A window that Explorer itself hosts only gets `WM_CLOSE`.
6. Critical processes (`dwm.exe`, `csrss.exe`, `lsass.exe`, and the rest of that set) are refused.

## Known limitations

Classic Control Panel pages that Explorer hosts in a second process — **Program och funktioner** / **Programs and Features** is the usual one — often show up on the taskbar as **Kontrollpanelen** / **Control Panel**. TaskbarEnd does not special-case those. Settings and ordinary desktop apps are the supported path.

Some apps restart themselves after they are killed (Steam is the common example). Windows 11’s own End task has the same limit.

## Antivirus

The injector uses `CreateRemoteThread` and `LoadLibrary` inside `explorer.exe`. That technique is also used by malware, so some scanners flag the injector or the DLL. The source in this repository is what those binaries are built from. If a scanner complains, allow `%LOCALAPPDATA%\TaskbarEnd\`.

## Layout

```
src/common/     log file
src/hook/       code that runs inside explorer.exe
src/injector/   loads and unloads the hook
scripts/        build, setup, uninstall
```

## License

[MIT](LICENSE). Copyright (c) 2026 Emil Ljungström.
