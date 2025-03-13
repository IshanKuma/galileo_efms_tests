#include "retentioncontroller.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <ddsretentionpolicy.hpp>

RetentionController::RetentionController(
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& retentionPolicy,
    const std::string& logFilePath, 
    const std::string& source) 
    : retentionPolicy(retentionPolicy) {
    try {
        logger = LoggingService::getInstance(source, logFilePath);
        logger->info("RetentionController initialization started", nlohmann::json::object(), "EFMS", true, "05001");
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize logging service: " + std::string(e.what()));
    }
}

void RetentionController::applyRetentionPolicy() {
    try {
        logger->info("Applying Retention Policy...", nlohmann::json::object(), "EFMS", true, "05001");

        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            logger->info("DDS_PATH found", nlohmann::json{{"value", ddsPathIt->second.at("value")}}, "EFMS", true, "05001");
        } else {
            logger->critical("DDS_PATH is missing in the retention policy!", nlohmann::json::object(), "EFMS", true, "05001");
            return;
        }

        auto pathIt = ddsPathIt->second.find("value");
        if (pathIt == ddsPathIt->second.end()) {
            logger->critical("'value' is missing under DDS_PATH!", nlohmann::json::object(), "EFMS", true, "05001");
            return;
        }

        if (!fileService.is_mounted_drive_accessible(pathIt->second)) {
            logger->critical("Drive is not accessible", nlohmann::json{{"drive", pathIt->second}}, "EFMS", true, "05001");
            return;
        }

        logger->info("Checking retention policy status", nlohmann::json::object(), "EFMS", true, "05001");
        if (checkRetentionPolicy()) {
            startMaxUtilizationPipeline();
        } else {
            startNormalPipeline();
        }
    } catch (const std::exception& e) {
        logger->critical("Error applying retention policy", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
    }
}

void RetentionController::startMaxUtilizationPipeline() {
    logger->info("Maximum Utilization Pipeline Started", nlohmann::json::object(), "EFMS", true, "05001");
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                if (checkRetentionPolicy() && checkFilePermissions(file)) {
                    logger->info("Deleting File", nlohmann::json{{"file", file}}, "EFMS", true, "05001");
                    fileService.delete_file(file);
                }
            }
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        logger->critical("Error in Maximum Utilization Pipeline", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
    }
}

void RetentionController::startNormalPipeline() {
    logger->info("Normal Pipeline Started", nlohmann::json::object(), "EFMS", true, "05001");
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            logger->info("Processing directory", nlohmann::json{{"directory", filePath}}, "EFMS", true, "05001");
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                if (isFileEligibleForDeletion(file) && checkFilePermissions(file)) {
                    logger->info("Deleting File", nlohmann::json{{"file", file}}, "EFMS", true, "05001");
                    fileService.delete_file(file);
                }
            }
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        logger->critical("Error in Normal Pipeline", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
    }
}

std::vector<std::string> RetentionController::getAllFilePaths() {
    std::vector<std::string> filepaths;
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
                logger->info("Found policy path", nlohmann::json{{policyKey, pathIt->second.at("value")}}, "EFMS", true, "05001");
            }
        } catch (const std::exception& e) {
            logger->error("Error retrieving file path", nlohmann::json{{"error", policyKey + " - " + e.what()}}, "EFMS", true, "05001");
        }
    }
    
    return filepaths;
}

double RetentionController::diskSpaceUtilization() {
    try {
        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            auto pathIt = ddsPathIt->second.find("value");
            if (pathIt != ddsPathIt->second.end()) {
                const std::string& path = pathIt->second;
                auto [totalMemory, usedMemory, freeMemory] = fileService.get_memory_details(path);
                
                if (totalMemory == 0) {
                    logger->critical("Total memory is zero", nlohmann::json{{"message", "Cannot calculate disk space utilization"}}, "EFMS", true, "05001");
                    return 0.0;
                }

                double utilization = static_cast<double>(usedMemory) / totalMemory * 100.0;
                logger->info("Disk space utilization calculated", nlohmann::json{{"utilization", std::to_string(utilization) + "%"}}, "EFMS", true, "05001");
                return utilization;
            }
            logger->critical("'PATH' key missing under 'DDS_PATH'", nlohmann::json::object(), "EFMS", true, "05001");
            return 0.0;
        }
        logger->critical("'DDS_PATH' key missing in retention policy", nlohmann::json::object(), "EFMS", true, "05001");
        return 0.0;
    } catch (const std::exception& e) {
        logger->critical("Error accessing disk space utilization", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
        return 0.0;
    }
}

bool RetentionController::checkRetentionPolicy() {
    try {
        double memoryUtilization = diskSpaceUtilization();
        int threshold = std::stoi(retentionPolicy.at("THRESHOLD_STORAGE_UTILIZATION").at("value"));
        bool exceeded = memoryUtilization > threshold;
        
        if (exceeded) {
            logger->info("Storage threshold exceeded", nlohmann::json{
                {"Utilization", memoryUtilization}, {"Threshold", threshold}
            }, "EFMS", true, "05001");
        }
        
        return exceeded;
    } catch (const std::exception& e) {
        logger->critical("Error checking retention policy", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
        return false;
    }
}

bool RetentionController::isFileEligibleForDeletion(const std::string& filePath) {
    const std::vector<std::pair<std::string, std::string>> policyMappings = {
        {"/Videos/", "VIDEO_RETENTION_POLICY"},
        {"/Analysis/", "PARQUET_RETENTION_POLICY"},
        {"/Diagnostics/", "DIAGNOSTIC_RETENTION_POLICY"},
        {"/Logs/", "LOG_RETENTION_POLICY"},
        {"/VideoClips/", "VIDEO_CLIPS_RETENTION_POLICY"}
    };

    try {
        auto policyIt = std::find_if(policyMappings.begin(), policyMappings.end(),
            [&filePath](const auto& mapping) {
                return filePath.find(mapping.first) != std::string::npos;
            });

        if (policyIt == policyMappings.end()) {
            logger->info("No matching policy found for file", nlohmann::json{{"file", filePath}}, "EFMS",true, "05001");
            return false;
        }

        ddsretentionpolicy retentionPolicyObj;
        auto policyDict = retentionPolicyObj.to_dict();
        std::string policyName = policyIt->second;
        std::string retentionPolicyPath = policyName + "_PATH";
        
        auto pathIt = policyDict.find(retentionPolicyPath);
        if (pathIt == policyDict.end()) {
            logger->error("Policy path not found", nlohmann::json{{"policy", retentionPolicyPath}}, "EFMS", true, "05001");
            return false;
        }

        int retentionPeriod = 0;
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

        int fileAge = fileService.get_file_age_in_hours(filePath);
        
        logger->info("Checking file eligibility", nlohmann::json{
            {"file", filePath}, {"age_hours", fileAge}, {"retention_hours", retentionPeriod}
        }, "default_message", false, "0");

        return fileAge > retentionPeriod;

    } catch (const std::exception& e) {
        logger->error("Error checking file eligibility", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
        return false;
    }
}

bool RetentionController::checkFilePermissions(const std::string& filePath) {
    bool hasPermission = fileService.check_file_permissions(filePath, Permission::DELETE);
    if (!hasPermission) {
        logger->warning("Insufficient permissions to delete file", nlohmann::json{{"file", filePath}}, "EFMS", true, "05001");
    }
    return hasPermission;
}

void RetentionController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            logger->info("Deleting Empty Directory", nlohmann::json{{"directory", directory}}, "EFMS", true, "05001");
            fileService.delete_directory(directory);
        }
    }
}
