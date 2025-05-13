#include "../include/archivalcontroller.hpp"
#include "fileservice.hpp"
#include "../include/logutils.hpp"
#include <iostream>
#include "databaseservice.hpp"
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <tuple>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <string>
#include "../include/vecowretentionpolicy.hpp"
#include <sstream>
#include <chrono>
#include <ctime>
#include "../include/db_instance.hpp"
#include <sys/prctl.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <map>

FileService fileService;

DatabaseUtilities& db = DatabaseUtilities::getInstance({
    "localhost",          
    "postgres",           
    "mysecretpassword",   
    "esk_galileo",        
    5432                  
});

// Global configuration variables
namespace ArchivalConfig {
    int bandwidth_limit_kb = 10240;  // Default value
    std::map<std::string, bool> eligibility;
    bool config_loaded = false;
    
    void loadConfig() {
        if (config_loaded) return;
        
        std::ifstream configFile("config.json");
        if (!configFile.is_open()) {
            return;
        }
        
        try {
            nlohmann::json config;
            configFile >> config;
            
            auto archival = config["archival"];
            bandwidth_limit_kb = archival["bandwidth_limit_kb"].get<int>();
            
            auto elig = archival["eligibility"];
            for (auto& [key, value] : elig.items()) {
                eligibility[key] = value.get<bool>();
            }
            
            config_loaded = true;
            
        } catch (const nlohmann::json::exception& e) {
            // Silent catch
        }
        
        configFile.close();
    }
}

ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                       const std::string& source,
                                       const std::string& logFilePath)
    : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
    try {
        // Load configuration at initialization
        ArchivalConfig::loadConfig();
        
        logger = LoggingService::getInstance(source, logFilePath);
        logger->info("ArchivalController initialization started",
                     createLogInfo({{"detail", "Initialization started successfully"}}),
                     "ARCH_INIT_START", false);

        if (!archivalPolicy.is_object()) {
            nlohmann::json errInfo = createLogInfo({{"detail", "Archival policy is not a valid JSON object"}});
            logger->critical("Invalid archival policy: not a JSON object", errInfo, "ARCH_POLICY_INVALID", true, "05001");
            logIncidentToDB("Invalid archival policy: not a JSON object", errInfo, "05001");
            throw std::runtime_error("Invalid archival policy: not a JSON object");
        }

        const std::vector<std::pair<std::string, nlohmann::json::value_t>> requiredFields = {
            {"MOUNTED_PATH", nlohmann::json::value_t::string},
            {"DDS_PATH", nlohmann::json::value_t::string},
            {"THRESHOLD_STORAGE_UTILIZATION", nlohmann::json::value_t::string}
        };

        for (const auto& [field, expected_type] : requiredFields) {
            if (!archivalPolicy.contains(field)) {
                throw std::runtime_error("Missing required field in archival policy: " + field);
            }
        }

        this->archivalPolicy = archivalPolicy;
    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({{"detail", e.what()}});
        logger->critical("Initialization failed", errInfo, "ARCH_INIT_FAIL", true, "05002");
        logIncidentToDB("Initialization failed", errInfo, "05002");
        throw;
    }
}

void ArchivalController::logIncidentToDB(const std::string& message, const nlohmann::json& details, const std::string& error_code) {
    try {
        // Check if the most recent incident with this message is still active (no recovery attempts or failed recovery)
        std::ostringstream checkQuery;
        checkQuery << "SELECT i.id FROM incident i "
                  << "LEFT JOIN recovery r ON i.id = r.incident_id "
                  << "WHERE i.incident_message = '" << message << "' "
                  << "AND i.process_name = 'EFMS' "
                  << "AND (r.id IS NULL OR r.recovery_status = 'FAILED') "
                  << "ORDER BY i.incident_time DESC LIMIT 1";
        
        auto result = db.executeSelect(checkQuery.str());
        
        // If no active incident exists, insert a new one
        if (result.empty()) {
            // Ensure error_code is included in the details JSON
            nlohmann::json detailsWithCode = details;
            detailsWithCode["error_code"] = error_code;
            
            std::ostringstream insertQuery;
            insertQuery << "INSERT INTO incident (process_name, incident_message, incident_time, incident_details) "
                       << "VALUES ('EFMS', '" << message << "', NOW(), '" << detailsWithCode.dump() << "') "
                       << "RETURNING id";
            
            int lastInsertId = db.executeInsert(insertQuery.str());
            std::cout << "Inserted incident with ID: " << lastInsertId << " for error code: " << error_code << std::endl;
        } else {
            std::cout << "Skipped duplicate incident: " << message << " (already active or no successful recovery)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Database Operation Failed: " << e.what() << std::endl;
        logger->error("Database Operation Failed", {{"error", e.what()}}, "DB_OPERATION_FAIL", true, "05003");
    }
}

void ArchivalController::applyArchivalPolicy() {
    if (checkArchivalPolicy()) {
        logger->info("Starting max utilization pipeline",
                     createLogInfo({{"detail", "Storage threshold exceeded"}}),
                     "PIPELINE_MAX_START", false);
        startMaxUtilizationPipeline();
    } else {
        logger->info("Starting Normal utilization pipeline",
                     createLogInfo({{"detail", "Normal pipeline processing initiated"}}),
                     "PIPELINE_NORMAL_START", false);
        startNormalPipeline();
    }
}

void ArchivalController::startMaxUtilizationPipeline() {
    auto filePaths = getAllFilePaths();
    for (const auto& filePath : filePaths) {
        auto [files, directories] = fileService.read_directory_recursively(filePath);
        for (const auto& file : files) {
            if (checkArchivalPolicy()) {
                fileService.delete_file(file);
            } else {
                break;
            }
        }
        stopPipeline(directories);
    }
}

void ArchivalController::startNormalPipeline() {
    auto filePaths = getAllFilePaths();

    for (const auto& filePath : filePaths) {
        std::cout << "[DEBUG] Checking path: " << filePath << std::endl;

        if (!std::filesystem::exists(filePath)) {
            std::cerr << "[WARNING] Path does not exist: " << filePath << std::endl;
            continue;
        }

        auto [files, directories] = fileService.read_directory_recursively(filePath);

        for (const auto& file : files) {
            if (isFileEligibleForArchival(file)) {
                if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
                    nlohmann::json errInfo = createLogInfo({{"detail", "DDS path not accessible"}});
                    logger->error("DDS path not accessible", errInfo, "DDS_PATH_ERR", true, "05004");
                    logIncidentToDB("DDS path not accessible", errInfo, "05004");
                    return;
                }

                auto destinationPath = getDestinationPath(file);
                if (!isFileArchivedToDDS(file)) {
                    logger->info("Archiving file", createLogInfo({{"destination", destinationPath}}), "FILE_ARCHIVE", false);
                    fileService.copy_files(file, destinationPath, ArchivalConfig::bandwidth_limit_kb);
                    updateFileArchivalStatus(file, destinationPath);
                }
            }

            if (isFileEligibleForDeletion(file)) {
                fileService.delete_file(file);
            }
        }

        stopPipeline(directories);
    }
}

void ArchivalController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            fileService.delete_directory(directory);
        }
    }
}

bool ArchivalController::checkArchivalPolicy() {
    try {
        auto memoryUtilization = diskSpaceUtilization();
        if (!archivalPolicy.contains("THRESHOLD_STORAGE_UTILIZATION")) {
            logger->critical("Missing Threshold Configuration",
                             createLogInfo({{"memory_utilization", memoryUtilization}}),
                             "THRESHOLD_EXCEEDED", false, "05005");
            return false;
        }
        int threshold = std::stoi(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"].get<std::string>());
        return memoryUtilization > threshold;
    } catch (const std::exception& e) {
        logger->error("Failed to check archival policy", createLogInfo({{"detail", e.what()}}), "CHECK_POLICY_FAIL", true, "05006");
        logIncidentToDB("Failed to check archival policy", createLogInfo({{"detail", e.what()}}), "05006");
        return false;
    }
}

std::vector<std::string> ArchivalController::getAllFilePaths() {
    std::vector<std::string> paths;
    try {
        std::cout << "[DEBUG] Starting to extract retention policy paths...\n";

        for (const auto& [key, value] : archivalPolicy.items()) {
            std::cout << "[DEBUG] Inspecting key: " << key << "\n";
            
            // Only process string values with RETENTION_POLICY in key and that look like paths
            if (value.is_string() && key.find("RETENTION_POLICY") != std::string::npos) {
                const std::string& path = value.get<std::string>();
                
                // Check if it looks like a path
                if (!path.empty() && (path[0] == '/' || path.find('/') != std::string::npos) && 
                    !(path.length() < 10 && path[0] >= 'a' && path[0] <= 'z')) {
                    std::cout << "[DEBUG] Found retention path for key " << key << ": " << path << "\n";
                    paths.push_back(path);
                } else {
                    std::cout << "[DEBUG] Skipping key " << key << " (value doesn't look like a path)\n";
                }
            } else {
                std::cout << "[DEBUG] Skipping key " << key << " (not a string or not a retention policy)\n";
            }
        }

        std::cout << "[DEBUG] Total paths collected: " << paths.size() << "\n";
    } catch (const std::exception& e) {
        logger->critical("Failed to get file paths", createLogInfo({{"detail", e.what()}}), "GET_PATHS_FAIL", true, "05007");
        std::cerr << "[ERROR] Exception while getting file paths: " << e.what() << "\n";
    }

    return paths;
}

double ArchivalController::diskSpaceUtilization() {
    try {
        std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
        uint64_t total = 0, used = 0, free = 0;
        std::tie(total, used, free) = fileService.get_memory_details(mountedPath);
        if (total == 0) throw std::runtime_error("Invalid disk space information: total space is 0");
        return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
    } catch (const std::exception& e) {
        logger->critical("Failed to get disk space utilization", createLogInfo({{"detail", e.what()}}), "DISK_UTIL_FAIL", true, "05009");
        logIncidentToDB("Failed to get disk space utilization", createLogInfo({{"detail", e.what()}}), "05009");
        throw;
    }
}

double ArchivalController::checkFileArchivalPolicy(const std::string& filePath) {
    return fileService.get_file_age_in_hours(filePath);
}

bool ArchivalController::isFileEligibleForDeletion(const std::string& filePath) {
    double fileAge = checkFileArchivalPolicy(filePath);
    try {
        if (!archivalPolicy.contains("RETENTION_POLICIES")) return false;
        const auto& policies = archivalPolicy["RETENTION_POLICIES"];

        if (filePath.find("Videos") != std::string::npos && policies.contains("Videos")) {
            return fileAge > policies["Videos"].get<double>();
        } else if (filePath.find("Analysis") != std::string::npos && policies.contains("Analysis")) {
            return fileAge > policies["Analysis"].get<double>();
        } else if (filePath.find("Diagnostics") != std::string::npos && policies.contains("Diagnostics")) {
            return fileAge > policies["Diagnostics"].get<double>();
        } else if (filePath.find("Logs") != std::string::npos && policies.contains("Logs")) {
            return fileAge > policies["Logs"].get<double>();
        } else if (filePath.find("VideoClips") != std::string::npos && policies.contains("VideoClips")) {
            return fileAge > policies["VideoClips"].get<double>();
        }
    } catch (...) {}
    return false;
}

bool ArchivalController::isFileEligibleForArchival(const std::string& filePath) {
    // Check if config was loaded
    if (!ArchivalConfig::config_loaded || ArchivalConfig::eligibility.empty()) {
        return true;  // Default to eligible if config fails
    }
    
    // Use configured eligibility from config.json
    if (filePath.find("Videos") != std::string::npos) {
        return ArchivalConfig::eligibility["Videos"];
    } else if (filePath.find("Analysis") != std::string::npos) {
        return ArchivalConfig::eligibility["Analysis"];
    } else if (filePath.find("Diagnostics") != std::string::npos) {
        return ArchivalConfig::eligibility["Diagnostics"];
    } else if (filePath.find("Logs") != std::string::npos) {
        return ArchivalConfig::eligibility["Logs"];
    } else if (filePath.find("VideoClips") != std::string::npos) {
        return ArchivalConfig::eligibility["VideoClips"];
    }

    return false;
}

bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
    std::string query;
    if (filePath.find("Videos") != std::string::npos) {
        // Use COALESCE to handle NULL values
        query = "SELECT COALESCE(dds_video_file_location, '') as dds_location FROM analytics WHERE video_file_location = '" + filePath + "'";
    } else if (filePath.find("Analysis") != std::string::npos) {
        // Use COALESCE to handle NULL values  
        query = "SELECT COALESCE(dds_parquet_file_location, '') as dds_location FROM analytics WHERE parquet_file_location = '" + filePath + "'";
    } else {
        std::string mountedPath = archivalPolicy.at("MOUNTED_PATH").get<std::string>();
        std::string ddsPath = archivalPolicy.at("DDS_PATH").get<std::string>();
        std::string ddsFilePath = filePath;

        size_t pos = ddsFilePath.find(mountedPath);
        if (pos != std::string::npos) {
            ddsFilePath.replace(pos, mountedPath.length(), ddsPath);
        }

        return fileService.file_exists(ddsFilePath);
    }

    try {
        auto result = db.executeSelect(query);
        
        // If no rows returned, file is not archived
        if (result.empty()) {
            return false;
        }
        
        // Check if the DDS location is not empty
        for (const auto& row : result) {
            if (!row.empty()) {
                std::string ddsLocation = row[0];
                if (!ddsLocation.empty()) {
                    return true;  // File is archived if we have a non-empty DDS location
                }
            }
        }
        
        return false;  // File is not archived if DDS location is empty or NULL
        
    } catch (const std::exception& e) {
        // Log the error but don't throw - just return false
        logger->error("Error checking file archival status", 
                     createLogInfo({{"error", e.what()}, {"file", filePath}}), 
                     "ARCHIVE_CHECK_FAIL", false, "05010");
        return false;
    }
}

void ArchivalController::updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath) {
    std::string query;
    if (filePath.find("Videos") != std::string::npos) {
        query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath + "' WHERE video_file_location = '" + filePath + "'";
    } else if (filePath.find("Analysis") != std::string::npos) {
        query = "UPDATE analytics SET dds_parquet_file_location = '" + ddsFilePath + "' WHERE parquet_file_location = '" + filePath + "'";
    } else {
        return;
    }

    try {
        db.executeUpdate(query);
    } catch (const std::exception& e) {
        logger->error("Failed to update archival status", createLogInfo({{"error", e.what()}}), "ARCHIVE_UPDATE_FAIL", true, "05008");
        logIncidentToDB("Failed to update archival status", createLogInfo({{"error", e.what()}}), "05008");
    }
}

std::string ArchivalController::getDestinationPath(const std::string& filePath) {
    auto destinationPath = filePath;
    std::string mountedPath = archivalPolicy.at("MOUNTED_PATH").get<std::string>();
    size_t pos = filePath.find(mountedPath);
    if (pos != std::string::npos) {
        destinationPath.replace(pos, mountedPath.length(), archivalPolicy.at("DDS_PATH").get<std::string>());
    } else {
        logger->critical("Failed to create destination path",
                         createLogInfo({{"detail", "MOUNTED_PATH not found in filePath"}}),
                         "DEST_PATH_ERR", true, "05012");
        logIncidentToDB("Failed to create destination path", createLogInfo({{"detail", "MOUNTED_PATH not found in filePath"}}), "05012");
    }
    return destinationPath;
}