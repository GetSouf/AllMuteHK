#include "console_ui.h"

#include "audio.h"
#include "autostart.h"
#include "daemon.h"
#include "hotkey.h"

#include <cstdio>
#include <iostream>
#include <limits>
#include <locale>
#include <windows.h>

namespace amh {

namespace {
UiLanguage g_ui_language = UiLanguage::Ru;

const wchar_t* tr(const wchar_t* text) { return text; }
}  // namespace

void setup_console_utf8() {
    try {
        std::locale system_locale("");
        std::locale::global(system_locale);
        std::wcout.imbue(system_locale);
        std::wcin.imbue(system_locale);
        std::wcerr.imbue(system_locale);
    } catch (...) {
        // Keep default C locale.
    }
}

void ensure_console_attached() {
    if (GetConsoleWindow() == nullptr) {
        AllocConsole();
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio();
    }
    setup_console_utf8();
}

void set_ui_language(const std::string& code) {
    (void)code;
    g_ui_language = UiLanguage::En;
}

std::string get_ui_language_code() {
    return "en";
}

void print_help() {
    std::wcout << L"\nAllMuteHotkey — global mute for all microphones\n\n";
    std::wcout << L"Usage:\n";
    std::wcout << L"  AllMuteHotkey menu                Open numeric menu\n";
    std::wcout << L"  AllMuteHotkey run                 Run daemon mode (hidden)\n";
    std::wcout << L"  AllMuteHotkey status              Show hotkey, autostart and microphones\n";
    std::wcout << L"  AllMuteHotkey hotkey Ctrl+Shift+M Set global hotkey\n";
    std::wcout << L"  AllMuteHotkey hotkey Ctrl+Mouse4  Set mouse hotkey\n";
    std::wcout << L"  AllMuteHotkey help                Show this help\n\n";
    print_hotkey_examples();
    std::wcout << L"\n";
}

void print_first_run_hint() {
    std::wcout << tr(L"Hotkey is not configured yet.\n");
    std::wcout << L"1) AllMuteHotkey hotkey Ctrl+Shift+M\n";
    std::wcout << L"2) AllMuteHotkey install\n\n";
}

void print_status(const Config& config) {
    std::wcout << L"\n--- AllMuteHotkey Status ---\n";
    std::wcout << L"Daemon: " << (is_daemon_running() ? L"running" : L"stopped") << L"\n";
    std::wcout << L"Autostart: " << (is_autostart_enabled() ? L"enabled" : L"disabled") << L"\n";
    std::wcout << L"Notification sound: " << (config.notify_sound ? L"on" : L"off") << L"\n";

    if (config.configured) {
        Hotkey hotkey{config.hotkey_modifiers, config.hotkey_vk};
        std::wcout << L"Hotkey: " << format_hotkey(hotkey) << L"\n";
    } else {
        std::wcout << L"Hotkey: not set\n";
    }

    const auto mics = list_microphones();
    if (mics.empty()) {
        std::wcout << L"\nMicrophones: no active input devices found.\n";
        std::wcout << L"Connect a microphone or check Windows sound settings.\n";
        return;
    }

    std::wcout << L"\nMicrophones:\n";
    for (const auto& mic : mics) {
        std::wcout << L"  - " << mic.name << L": ";
        if (!mic.reachable) {
            std::wcout << L"unreachable\n";
        } else {
            std::wcout << (mic.muted ? L"muted" : L"on") << L"\n";
        }
    }
    std::wcout << L"Overall: " << (are_all_microphones_muted() ? L"all muted" : L"some are unmuted")
               << L"\n";
}

void print_menu(const Config& config) {
    std::wcout << L"\n=== AllMuteHotkey ===\n";
    std::wcout << L"1. Show status\n";
    std::wcout << L"2. Configure hotkey\n";
    std::wcout << L"3. Enable autostart and run daemon\n";
    std::wcout << L"4. Disable autostart and stop daemon\n";
    std::wcout << L"5. Start daemon\n";
    std::wcout << L"6. Stop daemon\n";
    std::wcout << L"7. Toggle mute/unmute\n";
    std::wcout << L"8. Toggle notification sound\n";
    std::wcout << L"9. Help\n";
    std::wcout << L"0. Exit\n\n";

    if (config.configured) {
        Hotkey hotkey{config.hotkey_modifiers, config.hotkey_vk};
        std::wcout << L"Current hotkey: " << format_hotkey(hotkey) << L"\n";
    } else {
        std::wcout << L"Current hotkey: not set\n";
    }
    std::wcout << L"Daemon: " << (is_daemon_running() ? L"running" : L"stopped") << L"\n";
    std::wcout << L"Autostart: " << (is_autostart_enabled() ? L"enabled" : L"disabled") << L"\n";
    std::wcout << L"Notification sound: " << (config.notify_sound ? L"on" : L"off") << L"\n\n";
}

void print_hotkey_examples() {
    std::wcout << L"Hotkey examples: Ctrl+Shift+M, Ctrl+Alt+F8, Win+Shift+V, Ctrl+Mouse4\n";
    std::wcout << L"Mouse keys: MouseLeft, MouseRight, MouseMiddle, Mouse4, Mouse5\n";
    std::wcout << L"At least one modifier is required: Ctrl, Alt, Shift or Win.\n";
}

std::wstring prompt_line(const std::wstring& label) {
    std::wcout << label;
    std::wstring value;
    std::getline(std::wcin, value);
    return value;
}

int prompt_menu_choice() {
    while (true) {
        std::wcout << L"Enter menu number: ";
        std::wstring line;
        if (!std::getline(std::wcin, line)) {
            return 0;
        }

        try {
            std::size_t pos = 0;
            const int value = std::stoi(line, &pos);
            if (pos == line.size()) {
                return value;
            }
        } catch (...) {
        }

        print_error(L"Enter a number from menu.");
    }
}

void print_error(const std::wstring& message) {
    std::wcerr << L"Error: " << message << L"\n";
}

void print_success(const std::wstring& message) {
    std::wcout << message << L"\n";
}

void print_info(const std::wstring& message) {
    std::wcout << message << L"\n";
}

}  // namespace amh
