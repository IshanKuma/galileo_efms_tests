#include "retentioncontroller.hpp"
#include "logutils.hpp"
#include <iostream>
#include <stdexcept>
#include "db_instance.hpp"  
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <ddsretentionpolicy.hpp>
#include <sstream>
#include <chrono>
#include <ctime>
#include <sys/prctl.h>
#include <unistd.h>
#include <cstring>

// Constructor: Initializes the retention controller, setting up logging and storing the retention policy.
RetentionController::RetentionController(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& retentionPolicy,
                                           const std::string& logFilePath, 
                                           const std::string& source) 
    : retentionPolicy(retentionPolicy) {
    try {
       
        // Get an instance of the logger.
        logger = LoggingService::getInstance(source, logFilePath);
        logger->info("RetentionController initialization started", 
                     createLogInfo({{"detail", "Initialization started successfully"}}), 
                     "RETEN_INIT_START");
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize logging service: " + std::string(e.what()));
    }
}

// Logs an incident to the database by inserting an incident record.
void RetentionController::logIncidentToDB(const std::string& message, const nlohmann::json& details, const std::string& error_code) {
    try {
        std::ostringstream query;
        query << "INSERT INTO incidents (incident_message, incident_time, incident_details, process_name, recovery_status) VALUES ("
              << "'" << message << "', NOW(), '" << details.dump() << "', 'EFMS', 'PENDING') RETURNING id";

        int lastInsertId = db.executeInsert(query.str());  // Executes the query and fetches the ID

        std::cout << "Inserted incident with ID: " << lastInsertId << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Database Insert Failed: " << e.what() << std::endl;
        logger->error("Database Insert Failed", {{"error", e.what()}}, "DB_INSERT_FAIL", true, "05013");
    }
}

// Applies the retention policy by verifying key configuration and choosing the appropriate pipeline.
void RetentionController::applyRetentionPolicy() {
    try {
        logger->info("Applying Retention Policy...", 
                     createLogInfo({{"detail", "Retention policy application initiated"}}));
        
        // Check if DDS_PATH exists in the retention policy.
        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            logger->info("DDS_PATH found", 
                         createLogInfo({{"detail", ddsPathIt->second.at("value")}}));
        } else {
            logger->critical("DDS_PATH is missing in the retention policy!", 
                 createLogInfo({{"detail", "Retention policy does not contain DDS_PATH"}}), 
                 "RETENTION_ERR", true, "05014");

            logIncidentToDB("DDS_PATH is missing in the retention policy!", 
                createLogInfo({{"detail", "Retention policy does not contain DDS_PATH"}}), 
                "05014");

            return;
        }
        
        // Ensure the 'value' key exists under DDS_PATH.
        auto pathIt = ddsPathIt->second.find("value");
        if (pathIt == ddsPathIt->second.end()) {
            logger->critical("'value' is missing under DDS_PATH!", 
                 createLogInfo({{"detail", "Retention policy for DDS_PATH does not include a value"}}), 
                 "RETENTION_ERR", true, "05015");

            logIncidentToDB("'value' is missing under DDS_PATH!", 
                createLogInfo({{"detail", "Retention policy for DDS_PATH does not include a value"}}), 
                "05015");

            return;
        }
        
        // Check if the drive specified by DDS_PATH is accessible.
        if (!fileService.is_mounted_drive_accessible(pathIt->second)) {
            logger->critical("DDS path not accessible", 
                 createLogInfo({{"detail", pathIt->second+"Path not accessible"}}), 
                 "RETENTION_ERR", true, "05016");

            logIncidentToDB("DDS path not accessible", 
                createLogInfo({{"detail", pathIt->second+"Path not accessible"}}), 
                "05016");

            return;
        }
        
        // Log that retention policy status check is beginning.
        logger->info("Checking retention policy status", 
                     createLogInfo({{"detail", "Starting retention status check"}}));
        // Choose pipeline based on disk utilization.
        if (checkRetentionPolicy()) {
            startMaxUtilizationPipeline();
        } else {
            startNormalPipeline();
        }
    } catch (const std::exception& e) {
        logger->critical("Error applying retention policy", 
                 createLogInfo({{"detail", e.what()}}), 
                 "RETENTION_ERR", true, "05017");

        logIncidentToDB("Error applying retention policy", 
                createLogInfo({{"detail", e.what()}}), 
                "05017");

    }
}

// Maximum Utilization Pipeline: Deletes files until the retention policy condition is met.
void RetentionController::startMaxUtilizationPipeline() {
    logger->info("Maximum Utilization Pipeline Started", 
                 createLogInfo({{"detail", "Max utilization pipeline initiated"}}));
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            // Recursively get all files and directories.
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                // Check retention policy and file deletion permissions before deleting.
                if (checkRetentionPolicy() && checkFilePermissions(file)) {
                    logger->info("Deleting File", createLogInfo({{"file", file}}));
                    fileService.delete_file(file);
                }
            }
            // Remove empty directories.
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        logger->critical("Error in Maximum Utilization Pipeline", 
                 createLogInfo({{"detail", e.what()}}), 
                 "RETENTION_ERR", true, "05018");

        logIncidentToDB("Error in Maximum Utilization Pipeline", 
                createLogInfo({{"detail", e.what()}}), 
                "05018");

    }
}

// Normal Pipeline: Processes directories to delete files eligible for deletion.
void RetentionController::startNormalPipeline() {
    logger->info("Normal Pipeline Started", 
                 createLogInfo({{"detail", "Normal pipeline initiated"}}));
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            logger->info("Processing directory", createLogInfo({{"directory", filePath}}));
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                if (isFileEligibleForDeletion(file) && checkFilePermissions(file)) {
                    logger->info("Deleting File", createLogInfo({{"file", file}}));
                    fileService.delete_file(file);
                }
            }
            // Clean up any empty directories.
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        logger->critical("Error in Normal Pipeline", 
                 createLogInfo({{"detail", e.what()}}), 
                 "RETENTION_ERR", true, "05019");

        logIncidentToDB("Error in Normal Pipeline", 
                createLogInfo({{"detail", e.what()}}), 
                "05019");

    }
}

// Retrieves all file paths from the retention policy configuration.
std::vector<std::string> RetentionController::getAllFilePaths() {
    std::vector<std::string> filepaths;
     // List of keys to check for file paths.
    const std::vector<std::string> policyKeys = {
        "VIDEO_RETENTION_POLICY_PATH",
        "PARQUET_RETENTION_POLICY_PATH", 
        "DIAGNOSTIC_RETENTION_POLICY_PATH",
        "LOG_RETENTION_POLICY_PATH", 
        "VIDEO_CLIPS_RETENTION_POLICY_PATH"
    };

    for (const auto& policyKey : policyKeys) {
        try {
            auto pathIt = retentionPolicy.find(policyKey);
            if (pathIt != retentionPolicy.end()) {
                filepaths.push_back(pathIt->second.at("value"));
                logger->info("Found policy path", 
                             createLogInfo({{"detail", policyKey + ": " + pathIt->second.at("value")}}));
            }
        } catch (const std::exception& e) {
           
              

        }
    }
    
    return filepaths;
}

// Calculates disk space utilization (%) using the DDS_PATH from the retention policy.
double RetentionController::diskSpaceUtilization() {
    try {
        // Retrieve DDS_PATH details from the retention policy.
        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            auto pathIt = ddsPathIt->second.find("value");
            if (pathIt != ddsPathIt->second.end()) {
                const std::string& path = pathIt->second;
                auto [totalMemory, usedMemory, freeMemory] = fileService.get_memory_details(path);
                
                if (totalMemory == 0) {
                    logger->critical("Total memory is zero", 
                                     createLogInfo({{"detail", "Cannot calculate disk space utilization"}}), 
                                     "RETENTION_ERR", true, "05020");
                    return 0.0;
                }

                double utilization = static_cast<double>(usedMemory) / totalMemory * 100.0;
                logger->info("Disk space utilization calculated", 
                             createLogInfo({{"Utilization", std::to_string(utilization) + "%"}}));
                return utilization;
            }
            logger->critical("'PATH' key missing under 'DDS_PATH'", 
                 createLogInfo({{"detail", "Retention policy for DDS_PATH is missing a 'value' key"}}), 
                 "RETENTION_ERR", true, "05021");

            logIncidentToDB("'PATH' key missing under 'DDS_PATH'", 
                createLogInfo({{"detail", "Retention policy for DDS_PATH is missing a 'value' key"}}), 
                "05021");

            return 0.0;
        }
        logger->critical("'DDS_PATH' key missing in retention policy", 
                 createLogInfo({{"detail", "Retention policy does not include DDS_PATH key"}}), 
                 "RETENTION_ERR", true, "05022");

        logIncidentToDB("'DDS_PATH' key missing in retention policy", 
                createLogInfo({{"detail", "Retention policy does not include DDS_PATH key"}}), 
                "05022");

        return 0.0;
    } catch (const std::exception& e) {
       
        return 0.0;
    }
}

// Checks if the disk space utilization exceeds the configured threshold.
bool RetentionController::checkRetentionPolicy() {
    try {
        double memoryUtilization = diskSpaceUtilization();
        int threshold = std::stoi(retentionPolicy.at("THRESHOLD_STORAGE_UTILIZATION").at("value"));
        bool exceeded = memoryUtilization > threshold;
        
        if (exceeded) {
            logger->info("Storage threshold exceeded", 
                         createLogInfo({{"Utilization", std::to_string(memoryUtilization) + "%, Threshold: " + std::to_string(threshold) + "%"}}));
        }
        
        return exceeded;
    } catch (const std::exception& e) {
        logger->critical("Error checking retention policy", 
                 createLogInfo({{"detail", e.what()}}), 
                 "RETENTION_ERR", true, "05023");

        logIncidentToDB("Error checking retention policy", 
                createLogInfo({{"detail", e.what()}}), 
                "05023");

        return false;
    }
}

// Determines whether a file is eligible for deletion based on its age and the matching retention policy.
bool RetentionController::isFileEligibleForDeletion(const std::string& filePath) {
    const std::vector<std::pair<std::string, std::string>> policyMappings = {
        {"/Videos/", "VIDEO_RETENTION_POLICY"},
        {"/Analysis/", "PARQUET_RETENTION_POLICY"},
        {"/Diagnostics/", "DIAGNOSTIC_RETENTION_POLICY"},
        {"/Logs/", "LOG_RETENTION_POLICY"},
        {"/VideoClips/", "VIDEO_CLIPS_RETENTION_POLICY"}
    };

    try {
        // Find the appropriate retention policy mapping for the given file path.
        auto policyIt = std::find_if(policyMappings.begin(), policyMappings.end(),
            [&filePath](const auto& mapping) {
                return filePath.find(mapping.first) != std::string::npos;
            });

        if (policyIt == policyMappings.end()) {
            logger->info("No matching policy found for file", createLogInfo({{"detail", filePath}}));
            return false;
        }
        
        // Create an instance of the DDS retention policy object and retrieve its dictionary.
        ddsretentionpolicy retentionPolicyObj;
        auto policyDict = retentionPolicyObj.to_dict();
        std::string policyName = policyIt->second;
        std::string retentionPolicyPath = policyName + "_PATH";
        
        // Verify that the required policy path exists in the dictionary.
        auto pathIt = policyDict.find(retentionPolicyPath);
        if (pathIt == policyDict.end()) {
            
            return false;
        }

        int retentionPeriod = 0;
        // Determine the retention period based on file type.
        if (filePath.find("Videos") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::VIDEO_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("Analysis") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::PARQUET_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("Diagnostics") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::DIAGNOSTIC_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("Logs") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::LOG_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("VideoClips") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY.retentionPeriodInHours;
        }
           
        // Get the file's age in hours.
        int fileAge = fileService.get_file_age_in_hours(filePath);
        
        logger->info("Checking file eligibility", 
                     createLogInfo({{"file", filePath}, 
                                    {"age", std::to_string(fileAge) + " hours"}, 
                                    {"retention_period", std::to_string(retentionPeriod) + " hours"}}));
        // File is eligible for deletion if its age exceeds the retention period.
        return fileAge > retentionPeriod;

    } catch (const std::exception& e) {
        logger->error("Error checking file eligibility", 
              createLogInfo({{"detail", e.what()}}), 
              "RETENTION_ERR", true, "05024");

        logIncidentToDB("Error checking file eligibility", 
                createLogInfo({{"detail", e.what()}}), 
                "05024");

        return false;
    }
}

// Checks whether the file has the appropriate permissions for deletion.
bool RetentionController::checkFilePermissions(const std::string& filePath) {
    bool hasPermission = fileService.check_file_permissions(filePath, Permission::DELETE);
    if (!hasPermission) {
        logger->warning("Insufficient permissions to delete file", 
                createLogInfo({{"detail", filePath}}),"RETENTION_WARN",true,"05025");

        logIncidentToDB("Insufficient permissions to delete file", 
                createLogInfo({{"detail", filePath}}), 
                "05025");

    }
    return hasPermission;
}

// Stops the pipeline by deleting empty directories.
void RetentionController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            logger->info("Deleting Empty Directory", 
                         createLogInfo({{"detail", directory}}));
            fileService.delete_directory(directory);
        }
    }
}
