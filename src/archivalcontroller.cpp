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

// Global instance for FileService.
FileService fileService;

DatabaseUtilities& db = DatabaseUtilities::getInstance(
    DatabaseUtilities::ConnectionParams
    {
        "localhost",          
        "postgres",           
        "mysecretpassword",   
        "esk_galileo",        
        5432                  
    }
);

// Constructor for ArchivalController that initializes logging and validates the archival policy.
ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                         const std::string& source,
                                         const std::string& logFilePath)
    : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
    try {
           
        // Initialize the logger instance.   
        logger = LoggingService::getInstance(source, logFilePath);
        logger->info("ArchivalController initialization started", 
                     createLogInfo({ {"detail", "Initialization started successfully"} }),
                     "ARCH_INIT_START", false);
          
        // Validate that the archival policy is a JSON object.  
        if (!archivalPolicy.is_object()) {
            nlohmann::json errInfo = createLogInfo({ {"detail", "Archival policy is not a valid JSON object"} });
            logger->critical("Invalid archival policy: not a JSON object", 
                 errInfo, 
                 "ARCH_POLICY_INVALID", true, "05001");

            logIncidentToDB("Invalid archival policy: not a JSON object", 
                errInfo, 
                "05001");

            throw std::runtime_error("Invalid archival policy: not a JSON object");
        }
        
        // List of required fields with their expected JSON types.
        const std::vector<std::pair<std::string, nlohmann::json::value_t>> requiredFields = {
            {"MOUNTED_PATH", nlohmann::json::value_t::string},
            {"DDS_PATH", nlohmann::json::value_t::string},
            {"THRESHOLD_STORAGE_UTILIZATION", nlohmann::json::value_t::string}
        };
 
        // Check for the presence of each required field.
        for (const auto& [field, expected_type] : requiredFields) {
            if (!archivalPolicy.contains(field)) {
                throw std::runtime_error("Missing required field in archival policy: " + field);
            }
        }
        
        this->archivalPolicy = nlohmann::json(archivalPolicy);
        
    } catch (const std::exception& e) {
        // Log and rethrow any initialization errors.
        nlohmann::json errInfo = createLogInfo({ {"detail", e.what()} });
        logger->critical("Initialization failed", errInfo, "ARCH_INIT_FAIL", true, "05002");
        logIncidentToDB("Initialization failed", errInfo, "05002");
        throw;
    }
}

// Logs an incident to the database by inserting an entry into the incidents table.
void ArchivalController::logIncidentToDB(const std::string& message, const nlohmann::json& details, const std::string& error_code) {
    try {
        std::ostringstream query;
        // Build SQL query string with the incident details.
        query << "INSERT INTO incidents (incident_message, incident_time, incident_details, process_name, recovery_status) VALUES ("
              << "'" << message << "', NOW(), '" << details.dump() << "', 'EFMS', 'PENDING') RETURNING id";

        int lastInsertId = db.executeInsert(query.str());  // Executes the query and fetches the ID

        std::cout << "Inserted incident with ID: " << lastInsertId << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Database Insert Failed: " << e.what() << std::endl;
        logger->error("Database Insert Failed", {{"error", e.what()}}, "DB_INSERT_FAIL", true, "05003");
    }
}

// Applies the archival policy by determining which pipeline to start based on storage utilization.
void ArchivalController::applyArchivalPolicy() {
    if (checkArchivalPolicy()) {
        nlohmann::json info = createLogInfo({ {"detail", "Storage threshold exceeded"} });
        logger->info("Starting max utilization pipeline", info, "PIPELINE_MAX_START", false);
        startMaxUtilizationPipeline();
    } else {
        logger->info("Starting Normal utilization pipeline", 
                     createLogInfo({ {"detail", "Normal pipeline processing initiated"} }),
                     "PIPELINE_NORMAL_START", false);
        startNormalPipeline();
    }
}

// Pipeline for high storage utilization: Deletes files until policy conditions are met.
void ArchivalController::startMaxUtilizationPipeline() {
    auto filePaths = getAllFilePaths();
    for (const auto& filePath : filePaths) {
        auto [files, directories] = fileService.read_directory_recursively(filePath);

        for (const auto& file : files) {
        // Continue checking policy and delete files if threshold is still exceeded.    
            if (checkArchivalPolicy()) {
                fileService.delete_file(file);
            } else {
                break;
            }
        }
        // Remove empty directories.
        stopPipeline(directories);
    }
}

// Pipeline for normal utilization: Archives eligible files and deletes files based on retention policy.
void ArchivalController::startNormalPipeline() {
    auto filePaths = getAllFilePaths();
    for (const auto& filePath : filePaths) {
         // Recursively get files and directories.
        auto [files, directories] = fileService.read_directory_recursively(filePath);

        for (const auto& file : files) {
            // Check if the file is Eligible for archival.
            if (isFileEligibleForArchival(file)) {
               
                if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
                        nlohmann::json errInfo = createLogInfo({ {"detail", "DDS path not accessible: " + archivalPolicy.at("DDS_PATH").get<std::string>()} });
                        logger->error("DDS path not accessible", errInfo, "DDS_PATH_ERR", true, "05004");
                        logIncidentToDB("DDS path not accessible", errInfo, "05004");
                        return;
                    }
                // Generate destination path for the archival file.
                auto destinationPath = getDestinationPath(file);
                // If file has not been archived, perform archival.
                if (!isFileArchivedToDDS(file)) {
                    nlohmann::json info = createLogInfo({ {"destination", destinationPath} });
                    logger->info("Archiving file", info, "FILE_ARCHIVE", false);
                    fileService.copy_files(file, destinationPath, 10240);
                    updateFileArchivalStatus(file, destinationPath);
                }
            }
            // Delete file if it is eligible for deletion.
            if (isFileEligibleForDeletion(file)) {
                fileService.delete_file(file);
            }
        }
        // Remove empty directories.
        stopPipeline(directories);
    }
}

// Stops the pipeline by deleting empty directories.
void ArchivalController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            fileService.delete_directory(directory);
        }
    }
}

// Checks whether the archival policy condition is met based on current disk utilization.
bool ArchivalController::checkArchivalPolicy() {
    try {
        // Retrieve current disk space utilization.
        auto memoryUtilization = diskSpaceUtilization();

        if (!archivalPolicy.contains("THRESHOLD_STORAGE_UTILIZATION")) {
            nlohmann::json info = createLogInfo({ {"memory_utilization", memoryUtilization}});
            logger->critical("Missing Threshold Configuration", info, "THRESHOLD_EXCEEDED", false,"05005");
            return false;
        }
         
        // Convert threshold value from string to integer. 
        const auto& thresholdStr = static_cast<std::string>(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"]);
        int threshold = std::stoi(thresholdStr);

        if (memoryUtilization > threshold) {
            nlohmann::json info = createLogInfo({ {"memory_utilization", memoryUtilization}, {"threshold", threshold} });
            logger->info("Storage threshold exceeded", info, "THRESHOLD_EXCEEDED", false);
        }

        // Return true if storage utilization exceeds the threshold.
        return memoryUtilization > threshold;

    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"detail", e.what()} });
        logger->error("Failed to check archival policy", errInfo, "CHECK_POLICY_FAIL", true, "05006");
        logIncidentToDB("Failed to check archival policy", errInfo, "05006");

        return false;
    }
}

// Retrieves all file paths to be processed based on retention policy paths defined in the archival policy.
std::vector<std::string> ArchivalController::getAllFilePaths() {
    std::vector<std::string> paths;
    try {
        if (archivalPolicy.contains("VIDEO_RETENTION_POLICY_PATH")) {
            paths.push_back(archivalPolicy["VIDEO_RETENTION_POLICY_PATH"].get<std::string>());
        }
        if (archivalPolicy.contains("PARQUET_RETENTION_POLICY_PATH")) {
            paths.push_back(archivalPolicy["PARQUET_RETENTION_POLICY_PATH"].get<std::string>());
        }
        if (archivalPolicy.contains("DIAGNOSTIC_RETENTION_POLICY_PATH")) {
            paths.push_back(archivalPolicy["DIAGNOSTIC_RETENTION_POLICY_PATH"].get<std::string>());
        }
        if (archivalPolicy.contains("LOG_RETENTION_POLICY_PATH")) {
            paths.push_back(archivalPolicy["LOG_RETENTION_POLICY_PATH"].get<std::string>());
        }
        if (archivalPolicy.contains("VIDEO_CLIPS_RETENTION_POLICY_PATH")) {
            paths.push_back(archivalPolicy["VIDEO_CLIPS_RETENTION_POLICY_PATH"].get<std::string>());
        }
        return paths;
    } catch (const nlohmann::json::exception& e) {
        // Log critical error if there is an issue retrieving the paths.
        nlohmann::json errInfo = createLogInfo({ {"detail", e.what()} });
        logger->critical("Failed to get file paths", errInfo, "GET_PATHS_FAIL", true, "05007");
        throw;
    }
}

// Calculates disk space utilization as a percentage based on the mounted path.
double ArchivalController::diskSpaceUtilization() {
    try {
        if (!archivalPolicy.contains("MOUNTED_PATH")) {
            throw std::runtime_error("MOUNTED_PATH key not found in archival policy");
        }
        
        // Get the mounted path from the policy.
        std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
        uint64_t total = 0, used = 0, free = 0;
        // Get memory details (total, used, free) from FileService.
        std::tie(total, used, free) = fileService.get_memory_details(mountedPath);

        if (total == 0) {
            nlohmann::json errInfo = createLogInfo({ {"detail", "Total disk space is 0"} });
           logger->critical("Invalid disk space information", errInfo, "DISK_INFO_ERR", true, "05008");
           logIncidentToDB("Invalid disk space information", errInfo, "05008");

            throw std::runtime_error("Invalid disk space information: total space is 0");
        }
        // Calculate and return disk utilization percentage.
        return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;

    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"detail", e.what()} });
        logger->critical("Failed to get disk space utilization", errInfo, "DISK_UTIL_FAIL", true, "05009");
        logIncidentToDB("Failed to get disk space utilization", errInfo, "05009");
        throw;
    }
}

// Checks the age of the file in hours to determine if it meets the archival policy.
double ArchivalController::checkFileArchivalPolicy(const std::string& filePath) {
    return fileService.get_file_age_in_hours(filePath);
}


// Determines if a file is eligible for archival based on its type and configuration settings.
bool ArchivalController::isFileEligibleForArchival(const std::string& filePath) {

    // Path to the archival configuration JSON file.
    const std::string jsonFilePath = "configuration/archival_config.json";
    std::ifstream file(jsonFilePath);

    if (!file.is_open()) {
        nlohmann::json errInfo = createLogInfo({ {"detail","Not able to open the config.json"} });
        logger->warning("Could not open archival_config.json",errInfo, "", true, "05010");
        logIncidentToDB("Could not open archival_config.json", errInfo, "05010");
        throw std::runtime_error("Could not open archival_config.json");
    }

    nlohmann::json data;
    try {
        // Parse the configuration file.
        file >> data;
        // Determine archival eligibility based on file type keywords in the path.
        if (filePath.find("Videos") != std::string::npos) {
            return data.value("VideosArchival", false);
        } else if (filePath.find("Analysis") != std::string::npos) {
            return data.value("AnalysisArchival", false);
        } else if (filePath.find("Diagnostics") != std::string::npos) {
            return data.value("DiagnosticsArchival", false);
        } else if (filePath.find("Logs") != std::string::npos) {
            return data.value("LogsArchival", false);
        } else if (filePath.find("VideoClips") != std::string::npos) {
            return data.value("VideoclipsArchival", false);
        }
    } catch (const nlohmann::json::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"detail", e.what()} });
        logger->critical("Failed to parse archival config", errInfo, "PARSE_CONFIG_FAIL", true, "05011");
        logIncidentToDB("Failed to parse archival config", errInfo, "05011");

        throw;
    }

    return false;
}

// Determines if a file is eligible for deletion based on its age compared to its retention policy.
bool ArchivalController::isFileEligibleForDeletion(const std::string& filePath) 
{
    double fileAge = checkFileArchivalPolicy(filePath);
    double retentionPeriod;
    
    // Select the appropriate retention policy based on file type.
    if (filePath.find("Videos") != std::string::npos) {
        retentionPeriod = vecowretentionpolicy::VIDEO_RETENTION_POLICY.retentionPeriodInHours;
    } else if (filePath.find("Analysis") != std::string::npos) {
        retentionPeriod = vecowretentionpolicy::PARQUET_RETENTION_POLICY.retentionPeriodInHours;
    } else if (filePath.find("Diagnostics") != std::string::npos) {
        retentionPeriod = vecowretentionpolicy::DIAGNOSTIC_RETENTION_POLICY.retentionPeriodInHours;
    } else if (filePath.find("Logs") != std::string::npos) {
        retentionPeriod = vecowretentionpolicy::LOG_RETENTION_POLICY.retentionPeriodInHours;
    } else if (filePath.find("VideoClips") != std::string::npos) {
        retentionPeriod = vecowretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY.retentionPeriodInHours;
    } else {
        return false;
    }
    
    // File is eligible for deletion if its age exceeds the retention period.
    return fileAge > retentionPeriod;
}

bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
    auto query = "SELECT dds_video_file_location FROM analytics WHERE video_file_location = '" + filePath + "'";
    return false;
}

void ArchivalController::updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath) {
    auto query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath + "' WHERE video_file_location = '" + filePath + "'";
}

// Constructs the destination path for a file by replacing the mounted path with the DDS path.
std::string ArchivalController::getDestinationPath(const std::string& filePath) {
    auto destinationPath = filePath;
    // Retrieve the mounted path from the archival policy.
    std::string mountedPath = archivalPolicy.at("MOUNTED_PATH").get<std::string>();
    size_t pos = filePath.find(mountedPath);
    
    if (pos != std::string::npos) {
        destinationPath.replace(
            pos,
            mountedPath.length(),
            archivalPolicy.at("DDS_PATH").get<std::string>()
        );
    } else {
        nlohmann::json errInfo = createLogInfo({ {"detail", "MOUNTED_PATH not found in filePath"} });
        logger->critical("Failed to create destination path", errInfo, "DEST_PATH_ERR", true, "05012");
        logIncidentToDB("Failed to create destination path", errInfo, "05012");
    }
    return destinationPath;
}
