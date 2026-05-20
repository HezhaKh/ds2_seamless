#include "log.h"

#include <windows.h>
#include <cstdio>
#include <mutex>
#include <string>

namespace ds2sc::log {

namespace {
    HANDLE g_file = INVALID_HANDLE_VALUE;
    std::mutex g_mtx;

    void write_line(std::string_view level, std::string_view msg) {
        SYSTEMTIME st;
        GetLocalTime(&st);

        char buf[1024];
        const int n = std::snprintf(buf, sizeof(buf),
            "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [%.*s] %.*s\r\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            static_cast<int>(level.size()), level.data(),
            static_cast<int>(msg.size()), msg.data());

        if (n <= 0) return;

        std::lock_guard lock(g_mtx);
        if (g_file != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(g_file, buf, static_cast<DWORD>(n), &written, nullptr);
        }
        OutputDebugStringA(buf);
    }
}

void init(std::wstring_view dll_dir) {
    std::wstring path(dll_dir);
    path += L"\\ds2sc.log";

    std::lock_guard lock(g_mtx);
    if (g_file != INVALID_HANDLE_VALUE) return;

    g_file = CreateFileW(path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
}

void info(std::string_view m)  { write_line("INFO",  m); }
void warn(std::string_view m)  { write_line("WARN",  m); }
void error(std::string_view m) { write_line("ERROR", m); }

void shutdown() {
    std::lock_guard lock(g_mtx);
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
}

}
