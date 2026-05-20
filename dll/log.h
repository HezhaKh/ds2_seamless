#pragma once

#include <string_view>

namespace ds2sc::log {

void init(std::wstring_view dll_dir);
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);
void shutdown();

}
