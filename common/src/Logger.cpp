//
// Created by maxim on 12.07.2024.
//

#include "Logger.h"
Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
}

void Logger::setLogFile(const std::string &filename)
{
    m_logFile.open(filename, std::ios::app);
}

const char *Logger::getLogLevelString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::SERVER_ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

LogLevel Logger::parseLogLevel(const std::string &level)
{
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARNING") return LogLevel::WARNING;
    if (level == "ERROR") return LogLevel::SERVER_ERROR;
    if (level == "FATAL") return LogLevel::FATAL;
    return LogLevel::DEBUG; // Default to DEBUG if invalid level is provided
}
