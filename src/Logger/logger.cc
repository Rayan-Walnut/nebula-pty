#include "logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <sstream>
#include <thread>

// Initialisation des membres statiques
HANDLE Logger::hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
std::mutex Logger::consoleMutex;

void Logger::Debug(const std::string& message) {
    LogWithColor(message, "DEBUG", FOREGROUND_INTENSITY | FOREGROUND_BLUE);
}

void Logger::Info(const std::string& message) {
    LogWithColor(message, "INFO", FOREGROUND_INTENSITY | FOREGROUND_GREEN);
}

void Logger::Warning(const std::string& message) {
    LogWithColor(message, "WARNING", FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
}

void Logger::Error(const std::string& message) {
    LogWithColor(message, "ERROR", FOREGROUND_INTENSITY | FOREGROUND_RED);
}

void Logger::LogWithColor(const std::string& message, const std::string& level, WORD color) {
    std::time_t now = std::time(nullptr);
    char buffer[26];
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    std::stringstream threadId;
    threadId << std::this_thread::get_id();

    std::stringstream fullMessage;
    fullMessage << "[" << buffer << "] "
               << "[" << level << "] "
               << "[Thread: " << threadId.str() << "] "
               << message;

    // Log dans un fichier
    {
        static std::mutex fileMutex;
        std::lock_guard<std::mutex> lock(fileMutex);
        std::ofstream logFile("terminal.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << fullMessage.str() << std::endl;
        }
    }

    // Log dans la console avec couleur
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        SetConsoleTextAttribute(hConsole, color);
        std::cout << fullMessage.str() << std::endl;
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}