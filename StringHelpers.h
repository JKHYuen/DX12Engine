#pragma once

#include <stdexcept>
#include <string>

namespace StringConvert {
    // Source: https://stackoverflow.com/a/69410299
    inline std::wstring String_To_WideString(const std::string& string) {
        if(string.empty()) {
            return L"";
        }

        const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), nullptr, 0);
        if(size_needed <= 0) {
            throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
        }

        std::wstring result(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), result.data(), size_needed);
        return result;
    }

    inline std::string WideString_To_String(const std::wstring& wide_string) {
        if(wide_string.empty()) {
            return "";
        }

        const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
        if(size_needed <= 0) {
            throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
        }

        std::string result(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), result.data(), size_needed, nullptr, nullptr);
        return result;
    }
}

