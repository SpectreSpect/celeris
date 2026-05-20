#pragma once

#include <filesystem>
#include <string>

namespace path_utils {
    #if defined(_WIN32)
    #include <windows.h>
    #elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #else
    #include <unistd.h>
    #include <limits.h>
    #endif

    inline std::filesystem::path executable_path() {
    #if defined(_WIN32)
        wchar_t buf[32768];
        DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
        if (n == 0 || n == std::size(buf)) return {};
        return std::filesystem::path(buf);
    #elif defined(__APPLE__)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string tmp(size, '\0');
        if (_NSGetExecutablePath(tmp.data(), &size) != 0) return {};
        return std::filesystem::weakly_canonical(std::filesystem::path(tmp.c_str()));
    #else // Linux
        char buf[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n <= 0) return {};
        buf[n] = '\0';
        return std::filesystem::path(buf);
    #endif
    }

    inline std::filesystem::path executable_dir() {
        auto p = executable_path();
        return p.empty() ? std::filesystem::path{} : p.parent_path();
    }

    inline std::string executable_dir_str() {
        return executable_dir().string();
    }
}