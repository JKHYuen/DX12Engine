// Static logger class using WinAPI console for convenience
#pragma once

#include <format>
#include <iostream>
#include <DirectXMath.h>
#include <string>
#include <string_view>

class Logger {
public:
	// Static class
	Logger()                         = delete;
	Logger(const Logger&)            = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&)                 = delete;
	Logger& operator=(Logger&&)      = delete;

	// Create WinAPI console and reroute std io to it
	static void InitializeConsole();

	template<class T>
	static void Log(T msg) {
		std::cout << msg << std::endl;
	}

	static void Log(std::string_view label, DirectX::XMFLOAT3 msg) {
		std::cout << label << "(" << msg.x << ", " << msg.y << ", " << msg.z << ")" << std::endl;
	}

	static void Log(std::wstring_view msg) {
		std::wcout << msg << std::endl;
	}

	static void Log(std::wstring msg) {
		std::wcout << msg << std::endl;
	}

	template<class ...TArgs>
	static void Log(std::format_string<TArgs...> fmt, TArgs&&... args) {
		std::cout << std::format(fmt, std::forward<TArgs>(args)...) << std::endl;
	}

private:
	static constexpr int MAX_CONSOLE_LINES = 500;
};