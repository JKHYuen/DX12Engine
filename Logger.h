// Static logger class using WinAPI console for convenience
#pragma once

#include <iostream>
#include <format>

class Logger {
public:
	// Create WinAPI console and reroute std io to it
	static void InitializeConsole();

	template<class T>
	static void Log(T msg) {
		std::cout << msg << std::endl;
	}

	static void Log(std::wstring_view msg) {
		std::wcout << msg << std::endl;
	}

	template<class ...TArgs>
	static void Log(std::format_string<TArgs...> fmt, TArgs&&... args) {
		std::cout << std::format(fmt, std::forward<TArgs>(args)...) << std::endl;
	}

	Logger() = delete;

private:
	static constexpr int MAX_CONSOLE_LINES = 500;
};