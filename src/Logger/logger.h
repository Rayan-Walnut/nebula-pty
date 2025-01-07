// logger.h
#pragma once
#include <string>
#include <windows.h>
#include <mutex>

class Logger {
public:
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);

private:
    static void LogWithColor(const std::string& message, const std::string& level, WORD color);
    static HANDLE hConsole;
    static std::mutex consoleMutex;
};