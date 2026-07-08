#pragma once

#include "config.h"

#include <string>
#include <vector>

namespace amh {

enum class UiLanguage { Ru, En };

void setup_console_utf8();
void ensure_console_attached();
void set_ui_language(const std::string& code);
std::string get_ui_language_code();
void print_help();
void print_first_run_hint();
void print_status(const Config& config);
void print_menu(const Config& config);
void print_hotkey_examples();
std::wstring prompt_line(const std::wstring& label);
int prompt_menu_choice();
void print_error(const std::wstring& message);
void print_success(const std::wstring& message);
void print_info(const std::wstring& message);

}  // namespace amh
