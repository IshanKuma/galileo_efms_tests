#include "retentioncontroller.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm> // Add this header
#include <ddsretentionpolicy.hpp>

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
            std::cout << "Deleting File in retention policy: " << filePath << std::endl;
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
                std::cout << policyKey << ": " << pathIt->second.at("value") << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error retrieving file path for: " << policyKey 
                      << " - " << e.what() << std::endl;
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
    const std::vector<std::pair<std::string, std::string>> policyMappings = {
        {"/Videos/", "VIDEO_RETENTION_POLICY"},
        {"/Analysis/", "PARQUET_RETENTION_POLICY"},
        {"/Diagnostics/", "DIAGNOSTIC_RETENTION_POLICY"},
        {"/Logs/", "LOG_RETENTION_POLICY"},
        {"/VideoClips/", "VIDEO_CLIPS_RETENTION_POLICY"}
    };

    try {
        // Find matching policy based on file path
        auto policyIt = std::find_if(policyMappings.begin(), policyMappings.end(),
            [&filePath](const auto& mapping) {
                return filePath.find(mapping.first) != std::string::npos;
            });

        if (policyIt == policyMappings.end()) {
            return false;
        }

        // Get retention policy details
        ddsretentionpolicy retentionPolicyObj;
        auto policyDict = retentionPolicyObj.to_dict();

        // Find the specific policy's retention period
        std::string policyName = policyIt->second;
        std::string retentionPolicyPath = policyName + "_PATH";
        
        // Check if the specific policy path exists
        auto pathIt = policyDict.find(retentionPolicyPath);
        if (pathIt == policyDict.end()) {
            std::cerr << "Policy path not found: " << retentionPolicyPath << std::endl;
            return false;
        }

        // Get the retention period from the corresponding RetentionPolicy
        // std::string policyKey = policyName;
        // if (policyDict.find(policyKey) == policyDict.end()) {
        //     std::cerr << "Policy not found: " << policyKey << std::endl;
        //     return false;
        // }

        // Use the retention period from the specific policy
        int retentionPeriod = 0;
        if (filePath.find("Videos") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::VIDEO_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("Analysis") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::PARQUET_RETENTION_POLICY.retentionPeriodInHours;
        }else if (filePath.find("Diagnostics") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::DIAGNOSTIC_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("Logs") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::LOG_RETENTION_POLICY.retentionPeriodInHours;
        } else if (filePath.find("VideoClips") != std::string::npos) {
            retentionPeriod = ddsretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY.retentionPeriodInHours;
        }

        // Get file age and compare with retention period
        int fileAge = fileService.get_file_age_in_hours(filePath);

        // Optional logging for debugging
        std::cout << "File Path: " << filePath << std::endl;
        std::cout << "File Age: " << fileAge << " hours" << std::endl;
        std::cout << "Retention Period: " << retentionPeriod << " hours" << std::endl;

        // Determine if file is eligible for deletion
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
