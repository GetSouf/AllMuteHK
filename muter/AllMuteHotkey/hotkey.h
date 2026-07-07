#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <windows.h>

namespace amh {

struct Hotkey {
    std::uint32_t modifiers = 0;
    std::uint32_t vk = 0;
};

std::optional<Hotkey> parse_hotkey(const std::wstring& text);
std::wstring format_hotkey(const Hotkey& hotkey);
std::wstring hotkey_error_message(DWORD error_code);
bool is_mouse_hotkey_vk(std::uint32_t vk);

}  // namespace amh
