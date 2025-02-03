// LoggingService.cpp
#include "loggingservice.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>

// Initialize static members
LoggingService* LoggingService::instance = nullptr;
std::mutex LoggingService::mutex;

LoggingService* LoggingService::getInstance(const std::string& serviceName, 
                                          const std::string& logDir) {
    // Thread-safe initialization of singleton instance
    std::lock_guard<std::mutex> lock(mutex);
    if (instance == nullptr) {
        instance = new LoggingService(serviceName, logDir);
    }
    return instance;
}

LoggingService::LoggingService(const std::string& serviceName, const std::string& logDir)
    : serviceName(serviceName), logDirectory(logDir) {
    ensureLogDirectory();
    logFile = generateLogFilename();
    logStream.open(logFile, std::ios::app);  // Open in append mode
}

LoggingService::~LoggingService() {
    if (logStream.is_open()) {
        logStream.close();
    }
}

void LoggingService::ensureLogDirectory() {
    std::filesystem::create_directories(logDirectory);
}

std::string LoggingService::generateLogFilename() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << logDirectory << "/"
       << serviceName << "_"
       << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S")
       << ".log";
    return ss.str();
}

std::string LoggingService::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string LoggingService::formatLog(LogLevel level, const std::string& message,
                                    const std::string& additionalInfo) {
    std::string timestamp = getCurrentTimestamp();
    return "LOG " + timestamp + " | " + levelToString.at(level) + " | " +
           timestamp + " | " + message + " | " + additionalInfo + "\n";
}

void LoggingService::log(LogLevel level, const std::string& message,
                        const std::string& additionalInfo) {
    if (logStream.is_open()) {
        // Thread-safe logging operation
        std::lock_guard<std::mutex> lock(mutex);
        logStream << formatLog(level, message, additionalInfo);
        logStream.flush();  // Ensure immediate write to file
    }
}

// Implementation of convenience methods for each log level
void LoggingService::critical(const std::string& message, const std::string& additionalInfo) {
    log(LogLevel::CRIT, message, additionalInfo);
}

void LoggingService::error(const std::string& message, const std::string& additionalInfo) {
    log(LogLevel::ERROR, message, additionalInfo);
}

void LoggingService::warning(const std::string& message, const std::string& additionalInfo) {
    log(LogLevel::WARN, message, additionalInfo);
}

void LoggingService::info(const std::string& message, const std::string& additionalInfo) {
    log(LogLevel::INFO, message, additionalInfo);
}

void LoggingService::debug(const std::string& message, const std::string& additionalInfo) {
    log(LogLevel::DEBUG, message, additionalInfo);
}