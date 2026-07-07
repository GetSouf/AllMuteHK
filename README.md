# AllMuteHotkey

Lightweight Windows utility that toggles **all active microphones** with a global hotkey.

No GUI framework, no tray app framework, no heavy dependencies - just WinAPI + WASAPI.

## Features

- Toggle mute/unmute for all active input devices
- Global hotkeys from keyboard and mouse
- Console-only setup (menu + CLI commands)
- Optional autostart on Windows sign-in
- Hidden daemon mode (single-instance protection)
- Small top-center visual indicator for ~1 second on toggle:
  - `🎤` = unmuted
  - `🔇` = muted

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
| `help` | Show help |

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

## Architecture

```text
main.cpp       CLI + menu + command routing
daemon.cpp     hidden message window, hotkey/mouse hook loop, overlay indicator
audio.cpp      WASAPI microphone enumeration + mute control
hotkey.cpp     hotkey string parsing + formatting
config.cpp     config file in %APPDATA%\AllMuteHotkey\config.ini
autostart.cpp  autostart registry integration (HKCU\...\Run)
console_ui.cpp console output and menu rendering
```

## License

MIT
