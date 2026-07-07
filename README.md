# AllMuteHotkey

Lightweight Windows utility that toggles **all active microphones** with a global hotkey.

No heavy GUI framework, no external runtime dependencies - just WinAPI + WASAPI.

## Download Release

You can download a ready-to-run `.exe` from the project Releases page:

- [AllMuteHK Releases](https://github.com/GetSouf/AllMuteHK/releases)

After download:

1. Extract the archive (if the release is packaged).
2. Open PowerShell in the folder with `AllMuteHotkey.exe`.
3. Run:

```powershell
.\AllMuteHotkey.exe
```

Optional first-time setup:

```powershell
.\AllMuteHotkey.exe hotkey Ctrl+Shift+M
.\AllMuteHotkey.exe install
```

## Features

- Toggle mute/unmute for all active input devices
- Global hotkeys from keyboard and mouse
- Console-only setup (menu + CLI commands)
- Optional autostart on Windows sign-in
- Hidden daemon mode (single-instance protection)
- System tray icon with context menu (`Toggle mute`, `Exit`)
- Dynamic tray tooltip + icon state (`Muted` / `Unmuted`)
- Small top-center visual indicator for ~1 second on toggle:
  - `UNMUTED`
  - `MUTED`

## Quick Start

```powershell
# 1) Build Release x64 in Visual Studio
# 2) Set a hotkey
.\AllMuteHotkey.exe hotkey Ctrl+Shift+M

# 3) Enable autostart and launch daemon
.\AllMuteHotkey.exe install

# 4) Check current status
.\AllMuteHotkey.exe status
```

## Commands

| Command | Description |
|---------|-------------|
| `menu` | Open interactive numeric menu |
| `hotkey Ctrl+Shift+M` | Set global hotkey |
| `hotkey Ctrl+Mouse4` | Set mouse hotkey |
| `install` | Enable autostart and run daemon |
| `uninstall` | Disable autostart and stop daemon |
| `start` | Start daemon mode |
| `stop` | Stop daemon mode |
| `status` | Show hotkey, daemon/autostart state, microphones |
| `toggle` | Toggle all microphones |
| `mute` | Mute all microphones |
| `unmute` | Unmute all microphones |
| `notify on\|off` | Enable/disable beep on toggle |
| `lang en` | Keep English UI setting in config |
| `help` | Show help |

## Interactive Menu

When launched without arguments, the app opens a numeric menu:

- `1` Show status
- `2` Configure hotkey
- `3` Enable autostart and run daemon
- `4` Disable autostart and stop daemon
- `5` Start daemon
- `6` Stop daemon
- `7` Toggle mute/unmute
- `8` Toggle notification sound
- `9` Help
- `0` Exit

## Hotkey Format

- Modifiers: `Ctrl`, `Alt`, `Shift`, `Win` (at least one required)
- Keyboard main key: letters, digits, `F1`-`F24`, arrows, `Tab`, `Space`, etc.
- Mouse main key:
  - `MouseLeft`
  - `MouseRight`
  - `MouseMiddle`
  - `Mouse4` (`XBUTTON1`, usually side-back)
  - `Mouse5` (`XBUTTON2`, usually side-forward)

Examples:
- `Ctrl+Shift+M`
- `Ctrl+Alt+F8`
- `Win+Shift+V`
- `Ctrl+Mouse4`
- `Alt+Mouse5`

## Build

Requirements:
- Windows 10+
- Visual Studio 2022+ with **Desktop development with C++**

Steps:
1. Open `muter/muter.slnx`
2. Select `Release | x64`
3. Build Solution

Output binary:
- `muter/AllMuteHotkey/x64/Release/AllMuteHotkey.exe`

## Tray + Overlay UX

- Left/right click tray icon -> context menu
- `Toggle mute` toggles all active microphones
- `Exit` stops daemon cleanly
- Overlay appears at top-center for ~1 second after each toggle

## Architecture

```text
main.cpp       CLI + menu + command routing
daemon.cpp     hidden message window, hotkey/mouse hook loop, tray icon, overlay
audio.cpp      WASAPI microphone enumeration + mute control
hotkey.cpp     hotkey string parsing + formatting
config.cpp     config file in %APPDATA%\AllMuteHotkey\config.ini
autostart.cpp  autostart registry integration (HKCU\...\Run)
console_ui.cpp console output and menu rendering
```

## License

MIT
