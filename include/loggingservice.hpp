#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <map>
#include <mutex>

/**
 * @brief Enumeration for different log levels following Syslog standard
 * Levels are arranged in descending order of severity
 */
enum class LogLevel {
    CRIT,   // Critical conditions
    ERROR,  // Error conditions
    WARN,   // Warning conditions
    INFO,   // Informational messages
    DEBUG   // Debug-level messages
};

/**
 * @brief Thread-safe singleton logging service class
 * 
 * This class implements a singleton pattern for logging, ensuring that only
 * one instance of the logger exists across the entire application. It provides
 * thread-safe logging capabilities with different severity levels and formatted output.
 */
class LoggingService {
private:
    // Singleton instance pointer and mutex for thread safety
    static LoggingService* instance;
    static std::mutex mutex;
    
    // Core member variables
    std::string serviceName;    // Name of the service using the logger
    std::string logDirectory;   // Directory where log files are stored
    std::string logFile;        // Current log file path
    std::ofstream logStream;    // Output stream for writing logs
    
    /**
     * @brief Mapping of LogLevel enum to human-readable strings
     * Used for formatting log messages
     */
    const std::map<LogLevel, std::string> levelToString = {
        {LogLevel::CRIT, "CRIT"}, {LogLevel::ERROR, "ERROR"},
        {LogLevel::WARN, "WARN"},
        {LogLevel::INFO, "INFO"}, {LogLevel::DEBUG, "DEBUG"}
    };

    /**
     * @brief Private constructor to prevent direct instantiation
     * @param serviceName Name of the service using the logger
     * @param logDir Directory where log files will be stored
     */
    LoggingService(const std::string& serviceName, const std::string& logDir = "logs");
    
    /**
     * @brief Creates log directory if it doesn't exist
     */
    void ensureLogDirectory();

    /**
     * @brief Generates a unique log filename using service name and timestamp
     * @return String containing the generated filename
     */
    std::string generateLogFilename();

    /**
     * @brief Gets current timestamp for log entries
     * @return Formatted timestamp string
     */
    std::string getCurrentTimestamp();

    /**
     * @brief Formats log message according to specified format
     * Format: LOG TIMESTAMP | LOG LEVEL | OCCURRENCE TIMESTAMP | MESSAGE | ADDITIONAL INFO
     * @param level Severity level of the log
     * @param message Main log message
     * @param additionalInfo Additional context or metadata
     * @return Formatted log string
     */
    std::string formatLog(LogLevel level, const std::string& message, 
                         const std::string& additionalInfo);

public:
    // Prevent copying and assignment
    LoggingService(const LoggingService&) = delete;
    LoggingService& operator=(const LoggingService&) = delete;
    
    /**
     * @brief Destructor ensures log stream is properly closed
     */
    ~LoggingService();

    /**
     * @brief Gets singleton instance of the logger
     * If instance doesn't exist, creates it. If it exists, returns existing instance.
     * @param serviceName Name of the service (only used when creating new instance)
     * @param logDir Log directory (only used when creating new instance)
     * @return Pointer to the singleton logger instance
     */
    static LoggingService* getInstance(const std::string& serviceName = "DefaultService",
                                     const std::string& logDir = "logs");

    /**
     * @brief Core logging function used by all severity levels
     * @param level Severity level of the log
     * @param message Main log message
     * @param additionalInfo Additional context or metadata (optional)
     */
    void log(LogLevel level, const std::string& message, 
             const std::string& additionalInfo = "-");

    // Convenience methods for different log levels
    void critical(const std::string& message, const std::string& additionalInfo = "-");
    void error(const std::string& message, const std::string& additionalInfo = "-");
    void warning(const std::string& message, const std::string& additionalInfo = "-");
    void info(const std::string& message, const std::string& additionalInfo = "-");
    void debug(const std::string& message, const std::string& additionalInfo = "-");
};