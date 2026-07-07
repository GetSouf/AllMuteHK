#include "daemon.h"

#include "audio.h"
#include "hotkey.h"

#include <string>
#include <shellapi.h>
#include <windows.h>

namespace amh {

namespace {

struct DaemonState {
    Config config;
    HANDLE stop_event = nullptr;
};

DaemonState* g_state = nullptr;
HHOOK g_mouse_hook = nullptr;
HWND g_daemon_hwnd = nullptr;
constexpr UINT kMouseTriggerMessage = WM_APP + 1;
constexpr UINT kTrayCallbackMessage = WM_APP + 2;
constexpr UINT_PTR kTrayIconId = 1;
constexpr UINT kTrayMenuToggleId = 1001;
constexpr UINT kTrayMenuExitId = 1002;
constexpr wchar_t kOverlayWindowClassName[] = L"AllMuteHotkeyOverlayWindow";
constexpr UINT_PTR kOverlayTimerId = 1;
constexpr int kOverlayDurationMs = 1000;
constexpr int kOverlayWidth = 140;
constexpr int kOverlayHeight = 90;
constexpr int kOverlayTopOffset = 24;

HWND g_overlay_hwnd = nullptr;
HFONT g_overlay_font = nullptr;
bool g_overlay_muted = false;
NOTIFYICONDATAW g_tray_nid = {};

void show_overlay(bool muted);

void play_notify_sound(bool muted) {
    MessageBeep(muted ? MB_ICONASTERISK : MB_ICONHAND);
}

void update_tray_tooltip(bool muted) {
    if (g_tray_nid.hWnd == nullptr) {
        return;
    }
    std::wstring tip = L"AllMuteHotkey - ";
    tip += muted ? L"Muted" : L"Unmuted";
    wcsncpy_s(g_tray_nid.szTip, tip.c_str(), _TRUNCATE);
    g_tray_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_tray_nid);
}

void trigger_toggle() {
    if (g_state == nullptr) {
        return;
    }
    bool now_muted = false;
    if (toggle_all_microphones(now_muted)) {
        if (g_state->config.notify_sound) {
            play_notify_sound(now_muted);
        }
        show_overlay(now_muted);
        update_tray_tooltip(now_muted);
    }
}

void show_tray_menu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING, kTrayMenuToggleId, L"Toggle mute");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayMenuExitId, L"Exit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    const UINT selected =
        TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    switch (selected) {
        case kTrayMenuToggleId:
            trigger_toggle();
            break;
        case kTrayMenuExitId:
            if (g_state && g_state->stop_event) {
                SetEvent(g_state->stop_event);
            }
            break;
        default:
            break;
    }
}

bool create_tray_icon(HWND hwnd) {
    ZeroMemory(&g_tray_nid, sizeof(g_tray_nid));
    g_tray_nid.cbSize = sizeof(g_tray_nid);
    g_tray_nid.hWnd = hwnd;
    g_tray_nid.uID = kTrayIconId;
    g_tray_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray_nid.uCallbackMessage = kTrayCallbackMessage;
    g_tray_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(g_tray_nid.szTip, L"AllMuteHotkey", _TRUNCATE);
    return Shell_NotifyIconW(NIM_ADD, &g_tray_nid) == TRUE;
}

void remove_tray_icon() {
    if (g_tray_nid.hWnd != nullptr) {
        Shell_NotifyIconW(NIM_DELETE, &g_tray_nid);
        if (g_tray_nid.hIcon) {
            DestroyIcon(g_tray_nid.hIcon);
        }
        ZeroMemory(&g_tray_nid, sizeof(g_tray_nid));
    }
}

LRESULT CALLBACK overlay_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER:
            if (wparam == kOverlayTimerId) {
                KillTimer(hwnd, kOverlayTimerId);
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            HBRUSH background = CreateSolidBrush(RGB(25, 25, 25));
            FillRect(hdc, &rect, background);
            DeleteObject(background);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_overlay_muted ? RGB(255, 90, 90) : RGB(90, 220, 120));

            const wchar_t* icon_text = g_overlay_muted ? L"\U0001F507" : L"\U0001F3A4";
            DrawTextW(hdc, icon_text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool register_overlay_class() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = overlay_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kOverlayWindowClassName;
    return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void ensure_overlay_font() {
    if (g_overlay_font != nullptr) {
        return;
    }
    g_overlay_font = CreateFontW(
        52, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI Emoji");
}

void show_overlay(bool muted) {
    if (!register_overlay_class()) {
        return;
    }
    ensure_overlay_font();
    g_overlay_muted = muted;

    if (g_overlay_hwnd == nullptr) {
        g_overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayWindowClassName, L"", WS_POPUP, 0, 0,
            kOverlayWidth, kOverlayHeight, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!g_overlay_hwnd) {
            return;
        }
        if (g_overlay_font != nullptr) {
            SendMessageW(g_overlay_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_overlay_font), TRUE);
        }
    }

    const int x = (GetSystemMetrics(SM_CXSCREEN) - kOverlayWidth) / 2;
    const int y = kOverlayTopOffset;
    SetWindowPos(g_overlay_hwnd, HWND_TOPMOST, x, y, kOverlayWidth, kOverlayHeight,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_overlay_hwnd, nullptr, TRUE);

    KillTimer(g_overlay_hwnd, kOverlayTimerId);
    SetTimer(g_overlay_hwnd, kOverlayTimerId, kOverlayDurationMs, nullptr);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_HOTKEY:
            if (wparam == kHotkeyId && g_state != nullptr) {
                trigger_toggle();
            }
            return 0;
        case kMouseTriggerMessage:
            if (g_state != nullptr) {
                trigger_toggle();
            }
            return 0;
        case kTrayCallbackMessage:
            if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU || lparam == WM_LBUTTONUP) {
                show_tray_menu(hwnd);
            }
            return 0;
        case WM_DESTROY:
            remove_tray_icon();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool register_window_class() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;
    return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND create_message_window() {
    return CreateWindowExW(0, kWindowClassName, L"AllMuteHotkey", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
}

bool modifiers_match(std::uint32_t required_modifiers) {
    if ((required_modifiers & MOD_CONTROL) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
        return false;
    }
    if ((required_modifiers & MOD_SHIFT) && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        return false;
    }
    if ((required_modifiers & MOD_ALT) && !(GetAsyncKeyState(VK_MENU) & 0x8000)) {
        return false;
    }
    if ((required_modifiers & MOD_WIN) &&
        !((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))) {
        return false;
    }
    return true;
}

std::uint32_t mouse_message_to_vk(WPARAM wparam, LPARAM lparam) {
    const auto* data = reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
    switch (wparam) {
        case WM_LBUTTONDOWN:
            return VK_LBUTTON;
        case WM_RBUTTONDOWN:
            return VK_RBUTTON;
        case WM_MBUTTONDOWN:
            return VK_MBUTTON;
        case WM_XBUTTONDOWN: {
            if (data == nullptr) {
                return 0;
            }
            const DWORD button = HIWORD(data->mouseData);
            return button == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
        }
        default:
            return 0;
    }
}

LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && g_state != nullptr && g_daemon_hwnd != nullptr) {
        const std::uint32_t vk = mouse_message_to_vk(wparam, lparam);
        if (vk != 0 && vk == g_state->config.hotkey_vk &&
            modifiers_match(g_state->config.hotkey_modifiers)) {
            PostMessageW(g_daemon_hwnd, kMouseTriggerMessage, 0, 0);
        }
    }
    return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
}

}  // namespace

bool is_daemon_running() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, kMutexName);
    if (!mutex) {
        return false;
    }
    CloseHandle(mutex);
    return true;
}

bool signal_daemon_stop() {
    HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, kStopEventName);
    if (!event) {
        return false;
    }
    const BOOL ok = SetEvent(event);
    CloseHandle(event);
    return ok == TRUE;
}

int run_daemon(const Config& config) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex) {
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 2;
    }

    HANDLE stop_event = CreateEventW(nullptr, TRUE, FALSE, kStopEventName);
    if (!stop_event) {
        CloseHandle(mutex);
        return 1;
    }

    if (HWND console = GetConsoleWindow()) {
        ShowWindow(console, SW_HIDE);
    }

    DaemonState state{config, stop_event};
    g_state = &state;

    if (!register_window_class()) {
        CloseHandle(stop_event);
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }

    HWND hwnd = create_message_window();
    if (!hwnd) {
        CloseHandle(stop_event);
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }

    g_daemon_hwnd = hwnd;
    if (!create_tray_icon(hwnd)) {
        DestroyWindow(hwnd);
        g_daemon_hwnd = nullptr;
        CloseHandle(stop_event);
        ReleaseMutex(mutex);
        CloseHandle(mutex);
        return 1;
    }
    update_tray_tooltip(are_all_microphones_muted());

    const bool mouse_hotkey = is_mouse_hotkey_vk(config.hotkey_vk);
    if (mouse_hotkey) {
        g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, mouse_hook_proc, GetModuleHandleW(nullptr), 0);
        if (!g_mouse_hook) {
            DestroyWindow(hwnd);
            g_daemon_hwnd = nullptr;
            CloseHandle(stop_event);
            ReleaseMutex(mutex);
            CloseHandle(mutex);
            return 3;
        }
    } else {
        if (!RegisterHotKey(hwnd, kHotkeyId, config.hotkey_modifiers, config.hotkey_vk)) {
            DestroyWindow(hwnd);
            g_daemon_hwnd = nullptr;
            CloseHandle(stop_event);
            ReleaseMutex(mutex);
            CloseHandle(mutex);
            return 3;
        }
    }

    bool running = true;
    while (running) {
        const DWORD wait_result =
            MsgWaitForMultipleObjects(1, &stop_event, FALSE, INFINITE, QS_ALLINPUT);
        if (wait_result == WAIT_OBJECT_0) {
            break;
        }

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (mouse_hotkey) {
        if (g_mouse_hook) {
            UnhookWindowsHookEx(g_mouse_hook);
            g_mouse_hook = nullptr;
        }
    } else {
        UnregisterHotKey(hwnd, kHotkeyId);
    }
    DestroyWindow(hwnd);
    g_daemon_hwnd = nullptr;
    if (g_overlay_hwnd != nullptr) {
        DestroyWindow(g_overlay_hwnd);
        g_overlay_hwnd = nullptr;
    }
    if (g_overlay_font != nullptr) {
        DeleteObject(g_overlay_font);
        g_overlay_font = nullptr;
    }
    CloseHandle(stop_event);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    g_state = nullptr;
    return 0;
}

}  // namespace amh
