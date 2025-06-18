#ifndef MOCKS_HPP
#define MOCKS_HPP

#include "fileservice.hpp"
#include "databaseservice.hpp"
#include "loggingservice.hpp"
#include <string>
#include <vector>
#include <tuple>
#include <utility>
#include <map>

// Forward declarations for global mock pointers
class MockFileService;
class MockDatabaseUtilities;
class MockLoggingService;

extern MockFileService* g_mock_fs;
extern MockDatabaseUtilities* g_mock_db;
extern MockLoggingService* g_mock_logger;

// Mock for FileService
class MockFileService : public FileService {
public:
    // Example mock data members for test control
    std::tuple<uint64_t, uint64_t, uint64_t> memory_details = {0, 0, 0};
    bool mounted_drive_accessible = true;
    bool directory_empty = true;
    double file_age_hours = 0.0;
    std::vector<std::string> files;
    std::vector<std::string> directories;

    std::tuple<uint64_t, uint64_t, uint64_t> get_memory_details(const std::string& path) override {
        return memory_details;
    }
    bool is_mounted_drive_accessible(const std::string& path) override {
        return mounted_drive_accessible;
    }
    void copy_files(const std::string& source, const std::string& destination, int bandwidth_limit_kb) override {
        // No-op for mock
    }
    void delete_file(const std::string& file_path) override {
        // No-op for mock
    }
    bool is_directory_empty(const std::string& directory_path) override {
        return directory_empty;
    }
    void delete_directory(const std::string& directory_path) override {
        // No-op for mock
    }
    double get_file_age_in_hours(const std::string& file_path) override {
        return file_age_hours;
    }
    bool check_file_permissions(const std::string& file_path, Permission permission) override {
        return true;
    }
    std::pair<std::vector<std::string>, std::vector<std::string>> read_directory_recursively(const std::string& path) override {
        return {files, directories};
    }
};

// Mock for DatabaseUtilities
class MockDatabaseUtilities : public DatabaseUtilities {
public:
    // Add mock methods as needed for your tests
    // Example:
    bool insert(const std::string& table, const nlohmann::json& data) {
        return true;
    }
    // Add more methods as needed
};

// Mock for LoggingService
class MockLoggingService : public LoggingService {
public:
    MockLoggingService(const std::string& source, const std::string& logFilePath)
        : LoggingService(source, logFilePath) {}

    void log_info(const std::string& message) override {
        // No-op for mock
    }
    void log_warning(const std::string& message) override {
        // No-op for mock
    }
    void log_error(const std::string& message) override {
        // No-op for mock
    }
};

#endif // MOCKS_HPP