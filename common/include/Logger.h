//
// Created by maxim on 12.07.2024.
//

#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <string>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    SERVER_ERROR,
    FATAL
};

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger& getInstance();

    void setLogLevel(LogLevel level);

    void setLogFile(const std::string& filename);

    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* format, Args... args) {
        if (level >= m_logLevel) {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::string message = formatMessage(level, file, line, format, args...);
            std::cout << message;
            if (m_logFile.is_open()) {
                m_logFile << message;
                m_logFile.flush();
            }
        }
    }

    LogLevel parseLogLevel(const std::string& level);
private:
    Logger() : m_logLevel(LogLevel::INFO) {}


    template<typename... Args>
    std::string formatMessage(LogLevel level, const char* file, int line, const char* format, Args... args) {
        std::ostringstream oss;
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);

        std::tm time_info{};
#if defined(_WIN32)
        localtime_s(&time_info, &now_c);
#else
        localtime_r(&now_c, &time_info);
#endif

        oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S") << " ";
        oss << "[" << getLogLevelString(level) << "] ";
        oss << "[" << file << ":" << line << "] ";
        oss << formatString(format, args...) << std::endl;
        return oss.str();
    }

    template<typename... Args>
    std::string formatString(const char* format, Args... args) {
        int size = snprintf(nullptr, 0, format, args...);
        std::string result(size + 1, '\0');
        snprintf(&result[0], size + 1, format, args...);
        return result;
    }

    const char* getLogLevelString(LogLevel level);

    LogLevel m_logLevel;
    std::ofstream m_logFile;
    std::mutex m_mutex;
};

#define LOG_DEBUG(format, ...) Logger::getInstance().log(LogLevel::DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Logger::getInstance().log(LogLevel::INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) Logger::getInstance().log(LogLevel::WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::getInstance().log(LogLevel::SERVER_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) Logger::getInstance().log(LogLevel::FATAL, __FILE__, __LINE__, format, ##__VA_ARGS__)


