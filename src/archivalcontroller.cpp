#include "archivalcontroller.hpp"
#include "fileservice.hpp"
#include <iostream>
#include <fstream>  // Add this header
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>  // Add this header
#include <iomanip>  // for std::setprecision
#include <tuple>    // for std::tie
#include <cstdio>   // for popen, pclose
#include <stdexcept>
#include <vector>
#include <string>
/**
 * brief Global file service instance for handling file operations
 * 
 * This MockFileService instance is used throughout the ArchivalController
 * to perform various file system operations
 * Note: This is for Testing purpose
 */
FileService fileService;

/**
 * brief Constructor for ArchivalController
 * param archivalPolicy Map containing key-value pairs of archival settings and paths
 * param source Source identifier for the archival operation
 * param logFilePath Path where logs should be written
 * 
 * Initializes the controller with the provided archival policy settings
 */
ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                     const std::string& source,
                                     const std::string& logFilePath)
    : archivalPolicy(archivalPolicy) {
    try {
        std::cout << "Starting ArchivalController initialization..." << std::endl;
        
        // First, validate that we received a valid JSON object
        if (!archivalPolicy.is_object()) {
            std::cerr << "Initial archivalPolicy is not an object. Type: " << archivalPolicy.type_name() << std::endl;
            throw std::runtime_error("Invalid archival policy: not a JSON object");
        }

        // Print the entire policy for debugging
        std::cout << "Initial archival policy content:" << std::endl;
        std::cout << archivalPolicy.dump(2) << std::endl;

        // Validate required fields with type checking
        const std::vector<std::pair<std::string, nlohmann::json::value_t>> requiredFields = {
            {"MOUNTED_PATH", nlohmann::json::value_t::string},
            {"DDS_PATH", nlohmann::json::value_t::string},
            {"THRESHOLD_STORAGE_UTILIZATION",  nlohmann::json::value_t::string}
        };

        for (const auto& [field, expected_type] : requiredFields) {
            if (!archivalPolicy.contains(field)) {
                throw std::runtime_error("Missing required field in archival policy: " + field);
            }
            if (archivalPolicy[field].type() != expected_type) {
                std::cerr << "Field " << field << " has wrong type. Expected: " 
                         << static_cast<int>(expected_type) << " Got: " 
                         << static_cast<int>(archivalPolicy[field].type()) << std::endl;
                throw std::runtime_error("Invalid type for field: " + field);
            }
        }

        // Validate policy objects with detailed type checking
        // const std::vector<std::string> policyObjects = {
        //     "VIDEO_RETENTION_POLICY",
        //     "PARQUET_RETENTION_POLICY",
        //     "DIAGNOSTIC_RETENTION_POLICY",
        //     "LOG_RETENTION_POLICY",
        //     "VIDEO_CLIPS_RETENTION_POLICY"
        // };

        // for (const auto& policy : policyObjects) {
        //     if (!archivalPolicy.contains(policy)) {
        //         throw std::runtime_error("Missing policy object: " + policy);
        //     }
        //     if (!archivalPolicy[policy].is_object()) {
        //         std::cerr << "Policy " << policy << " is not an object. Type: " 
        //                  << archivalPolicy[policy].type_name() << std::endl;
        //         throw std::runtime_error("Invalid policy object type: " + policy);
        //     }
            
        //     // Validate retention period exists and is a number
        //     const auto& policyObj = archivalPolicy[policy];
        //     if (!policyObj.contains("RETENTION_PERIOD_IN_HOURS") || 
        //         !policyObj["RETENTION_PERIOD_IN_HOURS"].is_number()) {
        //         throw std::runtime_error("Invalid or missing RETENTION_PERIOD_IN_HOURS in " + policy);
        //     }
        // }

        std::cout << "ArchivalController initialization complete" << std::endl;
        std::cout << "MOUNTED_PATH: " << archivalPolicy["MOUNTED_PATH"].get<std::string>() << std::endl;
        
        // Make a deep copy of the policy to ensure we have our own instance
        this->archivalPolicy = nlohmann::json(archivalPolicy);
        
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON error during initialization: " << e.what() << std::endl;
        std::cerr << "JSON content: " << archivalPolicy.dump(2) << std::endl;
        throw;
    } catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        throw;
    }
}


/**
 * brief Main entry point for applying archival policies
 * 
 * Determines whether to use maximum utilization pipeline or normal pipeline
 * based on current storage conditions. This decision is made by checking
 * if storage utilization exceeds the defined threshold.
 */
void ArchivalController::applyArchivalPolicy() {
    std::cout << "Applying Archival Policy" << std::endl;

    if (checkArchivalPolicy()) {
        startMaxUtilizationPipeline();
    } else {
        startNormalPipeline();
    }
}

/**
 * brief Handles archival when storage utilization exceeds threshold
 * 
 * This aggressive pipeline is triggered when storage reaches critical levels.
 * It focuses on immediate file removal to free up space quickly, while still
 * maintaining archival integrity.
 */
void ArchivalController::startMaxUtilizationPipeline() {
    std::cout << "Max Utilization Pipeline Started as storage reached threshold" << std::endl;

    auto filePaths = getAllFilePaths();
    for (const auto& filePath : filePaths) {
        auto [files, directories] = fileService.read_directory_recursively(filePath);

        for (const auto& file : files) {
            if (checkArchivalPolicy()) {
                std::cout << "Deleting the file: " << file << std::endl;
                fileService.delete_file(file);
            } else {
                break;
            }
        }

        stopPipeline(directories);
    }
}

/**
 * brief Handles normal archival operations
 * 
 * This is the standard pipeline for archival operations when storage
 * utilization is below critical threshold. It handles both archiving
 * files to DDS and cleaning up old files based on retention policies.
 */
void ArchivalController::startNormalPipeline() {
    std::cout << "Normal Pipeline Started for archival" << std::endl;

    auto filePaths = getAllFilePaths();
    std::cout << "getAllFilePaths get called" << std::endl;
    for (const auto& filePath : filePaths) {
        std::cout << "inside for loop" << std::endl;
        auto [files, directories] = fileService.read_directory_recursively(filePath);
        std::cout << "read_directory_recursively" << std::endl;
        for (const auto& file : files) {
            if (isFileEligibleForArchival(file)) {
                if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
                    return;
                }

                auto destinationPath = getDestinationPath(file);
                if (!isFileArchivedToDDS(file)) {
                    fileService.copy_files(file, destinationPath, 10240);
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

/**
 * brief Cleans up empty directories after file processing
 * param directories Vector of directory paths to check and potentially remove
 * 
 * Iterates through provided directories and removes them if they're empty
 */
void ArchivalController::stopPipeline(const std::vector<std::string>& directories) {
    for (const auto& directory : directories) {
        if (fileService.is_directory_empty(directory)) {
            fileService.delete_directory(directory);
        }
    }
}

/**
 * brief Verifies if storage utilization exceeds configured threshold
 * return true if utilization exceeds threshold, false otherwise
 * 
 * Compares current disk space utilization against threshold defined
 * in archival policy
 */
bool ArchivalController::checkArchivalPolicy() {
    try {
        // First check if we can get the disk utilization
        auto memoryUtilization = diskSpaceUtilization();
        
        // Log current utilization for debugging
        std::cout << "Current disk utilization: " << memoryUtilization << "%" << std::endl;

        // Validate that archivalPolicy is a valid JSON object
        if (!archivalPolicy.is_object()) {
            std::cerr << "Error: archivalPolicy is not a valid JSON object" << std::endl;
            return false;
        }

        // Check if the threshold key exists
        if (!archivalPolicy.contains("THRESHOLD_STORAGE_UTILIZATION")) {
            std::cerr << "Error: THRESHOLD_STORAGE_UTILIZATION not found in policy" << std::endl;
            return false;
        }

        // Get and validate the threshold value
        const auto& thresholdStr = static_cast<std::string>(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"]);

        int threshold = std::stoi(thresholdStr);  // Convert to int
        // double thresholdValue = threshold.get<double>();

        // Return true if we're over threshold
        return memoryUtilization > threshold;

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON error in checkArchivalPolicy: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error in checkArchivalPolicy: " << e.what() << std::endl;
        return false;
    }
}

/**
 * brief Retrieves all monitored file paths
 * return Vector of paths that need to be processed for archival
 * 
 * Collects paths for videos, parquet files, diagnostics, logs,
 * and video clips from the archival policy
 */
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
        std::cerr << "JSON error in getAllFilePaths: " << e.what() << std::endl;
        std::cerr << "Current JSON structure: " << archivalPolicy.dump(2) << std::endl;
        throw;
    }
}

/**
 * brief Calculates current disk space utilization
 * return Percentage of disk space used (0-100)
 * 
 * Retrieves memory details from file service and calculates
 * the percentage of used space
 */
double ArchivalController::diskSpaceUtilization() {
    try {
        std::cout << "Calculating disk space utilization..." << std::endl;
        
        // Log the archival policy for debugging
        std::cout << "Archival policy content: " << archivalPolicy.dump(4) << std::endl;
        
        // Validate the presence and type of MOUNTED_PATH
        if (!archivalPolicy.contains("MOUNTED_PATH")) {
            throw std::runtime_error("MOUNTED_PATH key not found in archival policy");
        }
        if (!archivalPolicy["MOUNTED_PATH"].is_string()) {
            throw std::runtime_error("MOUNTED_PATH is not a valid string in archival policy");
        }

        std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
        std::cout << "Using MOUNTED_PATH: " << mountedPath << std::endl;
        
        uint64_t total = 0, used = 0, free = 0;

        // Try fetching memory details
        try {
            std::tie(total, used, free) = fileService.get_memory_details(mountedPath);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to get memory details: ") + e.what());
        }

        if (total == 0) {
            throw std::runtime_error("Invalid disk space information: total space is 0");
        }
        
        // Calculate disk utilization
        double utilization = (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
        std::cout << "Calculated disk utilization: " << std::fixed << std::setprecision(2) 
                  << utilization << "%" << std::endl;
        
        return utilization;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in diskSpaceUtilization: " << e.what() << std::endl;
        throw;
    }
}


double ArchivalController::checkFileArchivalPolicy(const std::string& filePath) {
    return fileService.get_file_age_in_hours(filePath);
}
/**
 * brief Determines if a file should be archived
 * param filePath Path to the file being evaluated
 * return true if file should be archived, false otherwise
 * 
 * Currently a placeholder that delegates to fileService
 */
bool ArchivalController::isFileEligibleForArchival(const std::string& filePath) {
    const std::string jsonFilePath = "../configuration/archival_config.json";
    std::ifstream file(jsonFilePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open archival_config.json");
    }

    nlohmann::json data;
    try {
        file >> data;
        std::cout << "Archival Status JSON exported" << std::endl;

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
        throw std::runtime_error("Error parsing archival_config.json: " + std::string(e.what()));
    }

    return false;
}

bool ArchivalController::isFileEligibleForDeletion(const std::string& filePath) {
    double fileAge = checkFileArchivalPolicy(filePath);
    double retentionPeriod;

    if (filePath.find("Videos") != std::string::npos) {
        retentionPeriod = archivalPolicy["VIDEO_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"].get<double>();
        std::cout << "----------retentionPeriod-------" << retentionPeriod << std::endl;
    } else if (filePath.find("Analysis") != std::string::npos) {
        retentionPeriod = archivalPolicy["PARQUET_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"].get<double>();
    } else if (filePath.find("Diagnostics") != std::string::npos) {
        retentionPeriod = archivalPolicy["DIAGNOSTIC_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"].get<double>();
    } else if (filePath.find("Logs") != std::string::npos) {
        retentionPeriod = archivalPolicy["LOG_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"].get<double>();
    } else if (filePath.find("VideoClips") != std::string::npos) {
        retentionPeriod = archivalPolicy["VIDEO_CLIPS_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"].get<double>();
    } else {
        return false;
    }

    return fileAge > retentionPeriod;
}

/**
 * brief Checks if a file has been archived to DDS
 * param filePath Path to the file being checked
 * return true if file is archived, false otherwise
 * 
 * Currently a placeholder for database query implementation
 */
bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
    auto query = "SELECT dds_video_file_location FROM analytics WHERE video_file_location = '" + filePath + "'";
    return 0; // Placeholder return
}

/**
 * brief Updates file archival status in database
 * param filePath Original path of the archived file
 * param ddsFilePath Path where file was archived in DDS
 * 
 * Currently a placeholder for database update implementation
 */
void ArchivalController::updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath) {
    auto query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath + "' WHERE video_file_location = '" + filePath + "'";
    // Implementation pending
}

/**
 * brief Generates destination path for file archival
 * param filePath Original path of the file to be archived
 * return Destination path in DDS storage
 * 
 * Replaces the mounted path prefix with DDS path prefix to determine
 * the archive destination while maintaining the rest of the path structure
 */
std::string ArchivalController::getDestinationPath(const std::string& filePath) {
    auto destinationPath = filePath;
    std::string mountedPath = archivalPolicy.at("MOUNTED_PATH").get<std::string>();
    size_t pos = filePath.find(mountedPath);
    
    if (pos != std::string::npos) {
        destinationPath.replace(
            pos,
            mountedPath.length(),
            archivalPolicy.at("DDS_PATH").get<std::string>()
        );
    } else {
        std::cerr << "Error: MOUNTED_PATH not found in filePath" << std::endl;
    }
    return destinationPath;
}