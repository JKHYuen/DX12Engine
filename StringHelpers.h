#pragma once

#include <stdexcept>
#include <string>

namespace StringConvert {
    // Source: https://stackoverflow.com/a/69410299
    inline void String_To_WideString(const std::string& string, std::wstring& out_wide_string) {
        if(string.empty()) {
            out_wide_string = L"";
            return;
        }

        const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), nullptr, 0);
        if(size_needed <= 0) {
            throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
        }

        out_wide_string = std::wstring(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, string.data(), (int)string.size(), out_wide_string.data(), size_needed);
    }

    inline void WideString_To_String(const std::wstring& wide_string, std::string& out_string) {
        if(wide_string.empty()) {
            out_string = "";
            return;
        }

        const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
        if(size_needed <= 0) {
            throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
        }

        out_string = std::string(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide_string.data(), (int)wide_string.size(), out_string.data(), size_needed, nullptr, nullptr);
    }
}

