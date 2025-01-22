#include "retentioncontroller.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <unordered_map>

RetentionController::RetentionController(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& retentionPolicy,
                                         const std::string& logFilePath, 
                                         const std::string& source) 
    : retentionPolicy(retentionPolicy) {
    // Validate if necessary policies and subkeys exist
    // validatePolicy();
}

// void RetentionController::validatePolicy() {
//     try {
//         const std::vector<std::string> requiredPolicies = {
//             "VIDEO_RETENTION_POLICY", "PARQUET_RETENTION_POLICY", "DIAGNOSTIC_RETENTION_POLICY",
//             "LOG_RETENTION_POLICY", "VIDEO_CLIPS_RETENTION_POLICY", "THRESHOLD_STORAGE_UTILIZATION", "DDS_PATH"
//         };

//         for (const auto& policy : requiredPolicies) {
//             if (retentionPolicy.find(policy) == retentionPolicy.end()) {
//                 throw std::invalid_argument("Missing required policy: " + policy);
//             }
//         }

//         const std::unordered_map<std::string, std::vector<std::string>> requiredSubKeys = {
//             {"VIDEO_RETENTION_POLICY", {"RETENTION_PERIOD_IN_HOURS", "VIDEO_PATH"}},
//             {"PARQUET_RETENTION_POLICY", {"RETENTION_PERIOD_IN_HOURS", "PARQUET_PATH"}},
//             {"DIAGNOSTIC_RETENTION_POLICY", {"RETENTION_PERIOD_IN_HOURS", "DIAGNOSTIC_PATH"}},
//             {"LOG_RETENTION_POLICY", {"RETENTION_PERIOD_IN_HOURS", "LOG_PATH"}},
//             {"VIDEO_CLIPS_RETENTION_POLICY", {"RETENTION_PERIOD_IN_HOURS", "VIDEO_CLIPS_PATH"}},
//             {"THRESHOLD_STORAGE_UTILIZATION", {"VALUE"}},
//             {"DDS_PATH", {"PATH"}}
//         };

//         for (const auto& [policyKey, subKeys] : requiredSubKeys) {
//             auto policyIt = retentionPolicy.find(policyKey);
//             if (policyIt == retentionPolicy.end()) {
//                 throw std::invalid_argument("Missing required policy: " + policyKey);
//             }

//             for (const auto& subKey : subKeys) {
//                 if (policyIt->second.find(subKey) == policyIt->second.end()) {
//                     throw std::invalid_argument("Missing key '" + subKey + "' in policy: " + policyKey);
//                 }
//             }
//         }

//     } catch (const std::exception& e) {
//         std::cerr << "Error during policy validation: " << e.what() << std::endl;
//         throw;  // Rethrow the exception to halt execution if validation fails
//     }
// }

void RetentionController::applyRetentionPolicy() {
    try {
        std::cout << "Applying Retention Policy..." << std::endl;

        // Check if DDS_PATH exists in the retention policy map
        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            std::cout << "DDS_PATH: " << ddsPathIt->second.at("value") << std::endl;
        } else {
            std::cerr << "Error: DDS_PATH is missing in the retention policy!" << std::endl;
            return;
        }

        // Ensure that the 'value' exists under DDS_PATH
        auto pathIt = ddsPathIt->second.find("value");
        if (pathIt == ddsPathIt->second.end()) {
            std::cerr << "Error: 'value' is missing under DDS_PATH!" << std::endl;
            return;
        }

        // Check if the drive is accessible
        if (!fileService.is_mounted_drive_accessible(pathIt->second)) {
            std::cerr << "Error: Drive is not accessible at: " << pathIt->second << std::endl;
            return;
        }

        std::cout << "Applying DDS Retention Policy" << std::endl;

        // Check retention policy and decide which pipeline to start
        if (checkRetentionPolicy()) {
            startMaxUtilizationPipeline();
        } else {
            startNormalPipeline();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error applying retention policy: " << e.what() << std::endl;
    }
}

void RetentionController::startMaxUtilizationPipeline() {
    std::cout << "Maximum Utilization Pipeline Started" << std::endl;
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                if (checkRetentionPolicy() && checkFilePermissions(file)) {
                    std::cout << "Deleting File: " << file << std::endl;
                    fileService.delete_file(file);
                }
            }
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in Maximum Utilization Pipeline: " << e.what() << std::endl;
    }
}

void RetentionController::startNormalPipeline() {
    std::cout << "Normal Pipeline Started" << std::endl;
    try {
        auto filepaths = getAllFilePaths();
        for (const auto& filePath : filepaths) {
            auto [files, directories] = fileService.read_directory_recursively(filePath);
            for (const auto& file : files) {
                if (isFileEligibleForDeletion(file) && checkFilePermissions(file)) {
                    std::cout << "Deleting File: " << file << std::endl;
                    fileService.delete_file(file);
                }
            }
            stopPipeline(directories);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in Normal Pipeline: " << e.what() << std::endl;
    }
}

std::vector<std::string> RetentionController::getAllFilePaths() {
    std::vector<std::string> filepaths;
    const std::vector<std::string> policies = {
        "VIDEO_RETENTION_POLICY", "PARQUET_RETENTION_POLICY", "DIAGNOSTIC_RETENTION_POLICY",
        "LOG_RETENTION_POLICY", "VIDEO_CLIPS_RETENTION_POLICY"
    };

    for (const auto& policy : policies) {
        try {
            // Use find() instead of at() to avoid throwing an exception if the policy is missing
            auto policyIt = retentionPolicy.find(policy);
            if (policyIt != retentionPolicy.end()) {
                const auto& policyMap = policyIt->second;

                // Check for the specific path under each policy
                if (policy == "VIDEO_RETENTION_POLICY") {
                    auto videoPathIt = policyMap.find("VIDEO_PATH");
                    if (videoPathIt != policyMap.end()) {
                        filepaths.push_back(videoPathIt->second);
                    }
                } else if (policy == "PARQUET_RETENTION_POLICY") {
                    auto parquetPathIt = policyMap.find("PARQUET_PATH");
                    if (parquetPathIt != policyMap.end()) {
                        filepaths.push_back(parquetPathIt->second);
                    }
                } else if (policy == "DIAGNOSTIC_RETENTION_POLICY") {
                    auto diagnosticPathIt = policyMap.find("DIAGNOSTIC_PATH");
                    if (diagnosticPathIt != policyMap.end()) {
                        filepaths.push_back(diagnosticPathIt->second);
                    }
                } else if (policy == "LOG_RETENTION_POLICY") {
                    auto logPathIt = policyMap.find("LOG_PATH");
                    if (logPathIt != policyMap.end()) {
                        filepaths.push_back(logPathIt->second);
                    }
                } else if (policy == "VIDEO_CLIPS_RETENTION_POLICY") {
                    auto videoClipsPathIt = policyMap.find("VIDEO_CLIPS_PATH");
                    if (videoClipsPathIt != policyMap.end()) {
                        filepaths.push_back(videoClipsPathIt->second);
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error retrieving file paths for policy: " << policy << " - " << e.what() << std::endl;
        }
    }
    return filepaths;
}


double RetentionController::diskSpaceUtilization() {
    try {
        // Find DDS_PATH and ensure "PATH" exists
        auto ddsPathIt = retentionPolicy.find("DDS_PATH");
        if (ddsPathIt != retentionPolicy.end()) {
            auto pathIt = ddsPathIt->second.find("value");
            if (pathIt != ddsPathIt->second.end()) {
                const std::string& path = pathIt->second;

                // Get memory details using the valid path
                auto [totalMemory, usedMemory, freeMemory] = fileService.get_memory_details(path);
                
                // Check to prevent division by zero and calculate disk space utilization
                if (totalMemory == 0) {
                    std::cerr << "Error: Total memory is zero, cannot calculate disk space utilization!" << std::endl;
                    return 0.0;
                }

                return static_cast<double>(usedMemory) / totalMemory * 100.0;
            } else {
                std::cerr << "Error: 'PATH' key missing under 'DDS_PATH'!" << std::endl;
                return 0.0;
            }
        } else {
            std::cerr << "Error: 'DDS_PATH' key missing in retention policy!" << std::endl;
            return 0.0;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error accessing disk space utilization: " << e.what() << std::endl;
        return 0.0;
    }
}


bool RetentionController::checkRetentionPolicy() {
    try {
        double memoryUtilization = diskSpaceUtilization();
        int threshold = std::stoi(retentionPolicy.at("THRESHOLD_STORAGE_UTILIZATION").at("value"));
        return memoryUtilization > threshold;
    } catch (const std::exception& e) {
        std::cerr << "Error checking retention policy: " << e.what() << std::endl;
        return false;
    }
}

bool RetentionController::isFileEligibleForDeletion(const std::string& filePath) {
    try {
        std::string retentionPolicyKey;

        if (filePath.find("/Videos/") != std::string::npos) {
            retentionPolicyKey = "VIDEO_RETENTION_POLICY";
        } else if (filePath.find("/Analysis/") != std::string::npos) {
            retentionPolicyKey = "PARQUET_RETENTION_POLICY";
        } else if (filePath.find("/Diagnostics/") != std::string::npos) {
            retentionPolicyKey = "DIAGNOSTIC_RETENTION_POLICY";
        } else if (filePath.find("/Logs/") != std::string::npos) {
            retentionPolicyKey = "LOG_RETENTION_POLICY";
        } else if (filePath.find("/VideoClips/") != std::string::npos) {
            retentionPolicyKey = "VIDEO_CLIPS_RETENTION_POLICY";
        } else {
            return false;
        }

        int fileAge = fileService.get_file_age_in_hours(filePath);
        int retentionPeriod = std::stoi(retentionPolicy.at(retentionPolicyKey).at("RETENTION_PERIOD_IN_HOURS"));
        return fileAge > retentionPeriod;
    } catch (const std::exception& e) {
        std::cerr << "Error checking file eligibility: " << e.what() << std::endl;
        return false;
    }
}

bool RetentionController::checkFilePermissions(const std::string& filePath) {
    return fileService.check_file_permissions(filePath, Permission::DELETE);
}

void RetentionController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            std::cout << "Deleting Empty Directory: " << directory << std::endl;
            fileService.delete_directory(directory);
        }
    }
}
