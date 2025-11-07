// Static logger class using WinAPI console for convenience
#pragma once

#include <iostream>

class Logger {
public:
	// Create WinAPI console and reroute std io to it
	static void InitializeConsole();

	static void Log(std::string_view msg);
	static void Log(std::wstring_view msg);

	Logger() = delete;

private:
	static constexpr int MAX_CONSOLE_LINES = 500;
};