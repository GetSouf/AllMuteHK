#include "autostart.h"
#include "audio.h"
#include "config.h"
#include "console_ui.h"
#include "daemon.h"
#include "hotkey.h"

#include <iostream>
#include <string>
#include <vector>
#include <cwctype>

#include <windows.h>

namespace {

std::wstring join_args(int argc, wchar_t** argv, int start_index) {
    std::wstring result;
    for (int i = start_index; i < argc; ++i) {
        if (!result.empty()) {
            result += L'+';
        }
        result += argv[i];
    }
    return result;
}

std::wstring to_lower(std::wstring text) {
    for (auto& ch : text) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return text;
}

bool is_help_command(const std::wstring& command) {
    return command == L"help" || command == L"/?" || command == L"-h" || command == L"--help";
}

bool validate_and_save_hotkey(amh::Config& config, const std::wstring& combo) {
    const auto parsed = amh::parse_hotkey(combo);
    if (!parsed) {
        amh::print_error(L"Failed to parse hotkey. Use format Ctrl+Shift+M, Ctrl+Alt+F8, etc.");
        amh::print_info(L"Use exactly one main key and at least one modifier.");
        return false;
    }

    HWND probe = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    if (!probe) {
        amh::print_error(L"Failed to validate hotkey.");
        return false;
    }

    if (!amh::is_mouse_hotkey_vk(parsed->vk)) {
        if (!RegisterHotKey(probe, amh::kHotkeyId, parsed->modifiers, parsed->vk)) {
            const DWORD error = GetLastError();
            DestroyWindow(probe);
            amh::print_error(amh::hotkey_error_message(error));
            return false;
        }
        UnregisterHotKey(probe, amh::kHotkeyId);
    }
    DestroyWindow(probe);

    config.hotkey_modifiers = parsed->modifiers;
    config.hotkey_vk = parsed->vk;
    config.configured = true;
    if (!amh::save_config(config)) {
        amh::print_error(L"Failed to save settings.");
        return false;
    }

    amh::print_success(std::wstring(L"Hotkey saved: ") + amh::format_hotkey(*parsed));
    if (amh::is_daemon_running()) {
        amh::print_info(L"Restart daemon mode to apply the new hotkey:");
        amh::print_info(L"  AllMuteHotkey stop");
        amh::print_info(L"  AllMuteHotkey start");
    } else {
        amh::print_info(L"Start daemon mode: AllMuteHotkey start");
    }
    return true;
}

bool start_background_process() {
    const std::wstring exe = amh::executable_path();
    if (exe.empty()) {
        amh::print_error(L"Failed to detect executable path.");
        return false;
    }

    std::wstring cmd = L"\"" + exe + L"\" run";
    std::vector<wchar_t> cmd_buffer(cmd.begin(), cmd.end());
    cmd_buffer.push_back(L'\0');
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(nullptr, cmd_buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &si, &pi)) {
        amh::print_error(L"Failed to start daemon mode.");
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

int run_menu() {
    amh::ensure_console_attached();

    while (true) {
        auto config = amh::load_config().value_or(amh::Config{});
        amh::set_ui_language("en");
        amh::print_menu(config);

        if (!config.configured) {
            amh::print_first_run_hint();
        }

        const int choice = amh::prompt_menu_choice();
        std::wcout << L"\n";

        switch (choice) {
            case 0:
                return 0;
            case 1:
                amh::print_status(config);
                break;
            case 2: {
                amh::print_hotkey_examples();
                const std::wstring combo = amh::prompt_line(L"Enter hotkey, e.g. Ctrl+Shift+M: ");
                if (combo.empty()) {
                    amh::print_info(L"Hotkey change cancelled.");
                    break;
                }
                validate_and_save_hotkey(config, combo);
                break;
            }
            case 3:
                if (!config.configured) {
                    amh::print_error(L"Set a hotkey first in menu item 2.");
                    break;
                }
                if (!amh::enable_autostart()) {
                    amh::print_error(L"Failed to enable autostart.");
                    break;
                }
                config.autostart = true;
                amh::save_config(config);
                if (!amh::is_daemon_running()) {
                    if (!start_background_process()) {
                        break;
                    }
                }
                amh::print_success(L"Autostart enabled. Daemon started.");
                break;
            case 4:
                if (amh::is_daemon_running()) {
                    amh::signal_daemon_stop();
                }
                if (!amh::disable_autostart()) {
                    amh::print_error(L"Failed to disable autostart.");
                    break;
                }
                config.autostart = false;
                amh::save_config(config);
                amh::print_success(L"Autostart disabled, daemon stopped.");
                break;
            case 5:
                if (!config.configured) {
                    amh::print_error(L"Set a hotkey first in menu item 2.");
                    break;
                }
                if (amh::is_daemon_running()) {
                    amh::print_info(L"Daemon is already running.");
                    break;
                }
                if (start_background_process()) {
                    amh::print_success(L"Daemon started.");
                }
                break;
            case 6:
                if (!amh::is_daemon_running()) {
                    amh::print_info(L"Daemon is not running.");
                    break;
                }
                if (!amh::signal_daemon_stop()) {
                    amh::print_error(L"Failed to stop daemon.");
                    break;
                }
                amh::print_success(L"Daemon stopped.");
                break;
            case 7: {
                bool now_muted = false;
                if (!amh::toggle_all_microphones(now_muted)) {
                    amh::print_error(L"Failed to toggle microphones. Check status.");
                    break;
                }
                amh::print_success(now_muted ? L"All microphones muted." : L"All microphones unmuted.");
                break;
            }
            case 8:
                config.notify_sound = !config.notify_sound;
                if (!amh::save_config(config)) {
                    amh::print_error(L"Failed to save settings.");
                    break;
                }
                amh::print_success(config.notify_sound ? L"Notification sound enabled."
                                                       : L"Notification sound disabled.");
                break;
            case 9:
                amh::print_help();
                break;
            default:
                amh::print_error(L"Invalid menu item.");
                break;
        }

        std::wcout << L"\n";
    }
}

int run_cli(int argc, wchar_t** argv) {
    amh::ensure_console_attached();

    if (argc < 2) {
        return run_menu();
    }

    const std::wstring command = to_lower(argv[1]);
    auto config = amh::load_config().value_or(amh::Config{});
    amh::set_ui_language("en");

    if (is_help_command(command)) {
        amh::print_help();
        return 0;
    }

    if (command == L"menu") {
        return run_menu();
    }

    if (command == L"lang") {
        if (argc < 3) {
            amh::print_error(L"Usage: AllMuteHotkey lang en");
            return 1;
        }
        const std::wstring value = to_lower(argv[2]);
        if (value == L"en") {
            config.language = "en";
        } else {
            amh::print_error(L"Usage: AllMuteHotkey lang en");
            return 1;
        }
        if (!amh::save_config(config)) {
            amh::print_error(L"Failed to save language.");
            return 1;
        }
        amh::set_ui_language(config.language);
        amh::print_success(L"Language switched to English.");
        return 0;
    }

    if (command == L"status") {
        amh::print_status(config);
        return 0;
    }

    if (command == L"hotkey") {
        if (argc < 3) {
            amh::print_error(L"Provide hotkey combo, for example: AllMuteHotkey hotkey Ctrl+Shift+M");
            return 1;
        }

        const std::wstring combo = join_args(argc, argv, 2);
        return validate_and_save_hotkey(config, combo) ? 0 : 1;
    }

    if (command == L"toggle") {
        bool now_muted = false;
        if (!amh::toggle_all_microphones(now_muted)) {
            amh::print_error(L"Failed to toggle microphones. Check status.");
            return 1;
        }
        amh::print_success(now_muted ? L"All microphones muted." : L"All microphones unmuted.");
        return 0;
    }

    if (command == L"mute") {
        if (!amh::set_all_microphones_muted(true)) {
            amh::print_error(L"Failed to mute microphones.");
            return 1;
        }
        amh::print_success(L"All microphones muted.");
        return 0;
    }

    if (command == L"unmute") {
        if (!amh::set_all_microphones_muted(false)) {
            amh::print_error(L"Failed to unmute microphones.");
            return 1;
        }
        amh::print_success(L"All microphones unmuted.");
        return 0;
    }

    if (command == L"notify") {
        if (argc < 3) {
            amh::print_error(L"Usage: AllMuteHotkey notify on|off");
            return 1;
        }
        const std::wstring value = to_lower(argv[2]);
        if (value == L"on" || value == L"1" || value == L"true") {
            config.notify_sound = true;
        } else if (value == L"off" || value == L"0" || value == L"false") {
            config.notify_sound = false;
        } else {
            amh::print_error(L"Usage: AllMuteHotkey notify on|off");
            return 1;
        }
        if (!amh::save_config(config)) {
            amh::print_error(L"Failed to save settings.");
            return 1;
        }
        amh::print_success(config.notify_sound ? L"Notification sound enabled."
                                               : L"Notification sound disabled.");
        return 0;
    }

    if (command == L"start" || command == L"run") {
        if (!config.configured) {
            amh::print_error(L"Set a hotkey first: AllMuteHotkey hotkey Ctrl+Shift+M");
            return 1;
        }
        if (amh::is_daemon_running()) {
            amh::print_info(L"Daemon is already running.");
            return 0;
        }

        if (command == L"start") {
            if (!start_background_process()) {
                return 1;
            }
            amh::print_success(L"Daemon mode started.");
            return 0;
        }

        const int code = amh::run_daemon(config);
        if (code == 2) {
            return 0;
        }
        if (code == 3) {
            amh::print_error(L"Hotkey is already used by another app. Pick another combination.");
            return 1;
        }
        if (code != 0) {
            amh::print_error(L"Failed to start daemon mode.");
            return 1;
        }
        return 0;
    }

    if (command == L"stop") {
        if (!amh::is_daemon_running()) {
            amh::print_info(L"Daemon is not running.");
            return 0;
        }
        if (!amh::signal_daemon_stop()) {
            amh::print_error(L"Failed to stop daemon mode.");
            return 1;
        }
        amh::print_success(L"Daemon mode stopped.");
        return 0;
    }

    if (command == L"install") {
        if (!config.configured) {
            amh::print_error(L"Set a hotkey first: AllMuteHotkey hotkey Ctrl+Shift+M");
            return 1;
        }
        if (!amh::enable_autostart()) {
            amh::print_error(L"Failed to enable autostart.");
            return 1;
        }
        config.autostart = true;
        amh::save_config(config);

        if (!amh::is_daemon_running()) {
            if (!start_background_process()) {
                return 1;
            }
        }

        amh::print_success(L"Autostart enabled. App will launch on Windows sign-in.");
        return 0;
    }

    if (command == L"uninstall") {
        if (amh::is_daemon_running()) {
            amh::signal_daemon_stop();
        }
        if (!amh::disable_autostart()) {
            amh::print_error(L"Failed to disable autostart.");
            return 1;
        }
        config.autostart = false;
        amh::save_config(config);
        amh::print_success(L"Autostart disabled, daemon stopped.");
        return 0;
    }

    amh::print_error(L"Unknown command: " + command);
    amh::print_info(L"Help: AllMuteHotkey help");
    return 1;
}

}  // namespace

int run_application(int argc, wchar_t** argv) {
    if (argc >= 2) {
        const std::wstring command = to_lower(argv[1]);
        if (command != L"run") {
            return run_cli(argc, argv);
        }
    } else if (argc == 1) {
        return run_cli(argc, argv);
    }

    auto config = amh::load_config().value_or(amh::Config{});
    if (!config.configured) {
        return 1;
    }

    const int code = amh::run_daemon(config);
    return code == 2 ? 0 : code;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }

    const int result = run_application(argc, argv);
    LocalFree(argv);
    return result;
}
