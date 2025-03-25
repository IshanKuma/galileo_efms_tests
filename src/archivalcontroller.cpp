// #include "archivalcontroller.hpp"
// #include "fileservice.hpp"
// #include <iostream>
// #include <fstream>
// #include <nlohmann/json_fwd.hpp>
// #include <nlohmann/json.hpp>
// #include <iomanip>
// #include <tuple>
// #include <cstdio>
// #include <stdexcept>
// #include <vector>
// #include <string>
// #include <vecowretentionpolicy.hpp>

// FileService fileService;

// ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
//                                          const std::string& source,
//                                          const std::string& logFilePath)
//     : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
//     try {
//         logger = LoggingService::getInstance(source, logFilePath);
//         // Use a JSON object for additional info even when there is no error detail.
//         logger->info("ArchivalController initialization started", { {"Initialization started successfully"} }, "ARCH_INIT_START");


//         if (!archivalPolicy.is_object()) {
//             nlohmann::json errInfo = { {"error_reason", "Archival policy is not a valid JSON object"} };
//             logger->critical("Invalid archival policy: not a JSON object", errInfo, "ARCH_POLICY_INVALID", true, "Invalid archival policy structure");
//             throw std::runtime_error("Invalid archival policy: not a JSON object");
//         }

//         const std::vector<std::pair<std::string, nlohmann::json::value_t>> requiredFields = {
//             {"MOUNTED_PATH", nlohmann::json::value_t::string},
//             {"DDS_PATH", nlohmann::json::value_t::string},
//             {"THRESHOLD_STORAGE_UTILIZATION", nlohmann::json::value_t::string}
//         };

//         for (const auto& [field, expected_type] : requiredFields) {
//             if (!archivalPolicy.contains(field)) {
//                 nlohmann::json errInfo = { {"error_reason", "Missing required field: " + field} };
//                 logger->critical("Missing required field: " + field, errInfo, "FIELD_MISSING_" + field, true, "Missing archival policy field");
//                 throw std::runtime_error("Missing required field in archival policy: " + field);
//             }
//         }
        
//         this->archivalPolicy = nlohmann::json(archivalPolicy);
        
//     } catch (const std::exception& e) {
//         nlohmann::json errInfo = { {"error_reason", e.what()} };
//         logger->critical("Initialization failed", errInfo, "ARCH_INIT_FAIL", true, e.what());
//         throw;
//     }
// }

// void ArchivalController::applyArchivalPolicy() {
//     if (checkArchivalPolicy()) {
//         // When storage threshold is exceeded, include a detail in the additional info.
//         nlohmann::json info = { {"detail", "Storage threshold exceeded"} };
//         logger->info("Starting max utilization pipeline", info, "PIPELINE_MAX_START", false, "");
//         startMaxUtilizationPipeline();
//     } else {
//         logger->info("Starting Normal utilization pipeline", { {"detail", "Normal pipeline processing initiated"} }, "PIPELINE_NORMAL_START", false, "");
//         startNormalPipeline();
//     }
// }

// void ArchivalController::startMaxUtilizationPipeline() {
//     auto filePaths = getAllFilePaths();
//     for (const auto& filePath : filePaths) {
//         auto [files, directories] = fileService.read_directory_recursively(filePath);

//         for (const auto& file : files) {
//             if (checkArchivalPolicy()) {
//                 fileService.delete_file(file);
//             } else {
//                 break;
//             }
//         }

//         stopPipeline(directories);
//     }
// }

// void ArchivalController::startNormalPipeline() {
//     auto filePaths = getAllFilePaths();
//     for (const auto& filePath : filePaths) {
//         auto [files, directories] = fileService.read_directory_recursively(filePath);

//         for (const auto& file : files) {
//             if (isFileEligibleForArchival(file)) {
//                 if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
//                     nlohmann::json errInfo = { {"error_reason", "DDS path not accessible: " + archivalPolicy.at("DDS_PATH").get<std::string>()} };
//                     logger->critical("DDS path not accessible", errInfo, "DDS_PATH_ERR", true, "DDS path inaccessible");
//                     return;
//                 }

//                 auto destinationPath = getDestinationPath(file);
//                 if (!isFileArchivedToDDS(file)) {
//                     nlohmann::json info = { {"destination", destinationPath} };
//                     logger->info("Archiving file", info, "FILE_ARCHIVE", false, "");
//                     fileService.copy_files(file, destinationPath, 10240);
//                     updateFileArchivalStatus(file, destinationPath);
//                 }
//             }

//             if (isFileEligibleForDeletion(file)) {
//                 fileService.delete_file(file);
//             }
//         }

//         stopPipeline(directories);
//     }
// }

// void ArchivalController::stopPipeline(const std::vector<std::string>& directories) {
//     for (const auto& directory : directories) {
//         if (fileService.is_directory_empty(directory)) {
//             fileService.delete_directory(directory);
//         }
//     }
// }

// bool ArchivalController::checkArchivalPolicy() {
//     try {
//         auto memoryUtilization = diskSpaceUtilization();

//         if (!archivalPolicy.contains("THRESHOLD_STORAGE_UTILIZATION")) {
//             nlohmann::json errInfo = { {"error_reason", "THRESHOLD_STORAGE_UTILIZATION not found in policy"} };
//             logger->critical("Missing threshold configuration", errInfo, "THRESHOLD_MISSING", true, "Threshold configuration missing");
//             return false;
//         }

//         const auto& thresholdStr = static_cast<std::string>(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"]);
//         int threshold = std::stoi(thresholdStr);

//         if (memoryUtilization > threshold) {
//             nlohmann::json info = {
//                 {"memory_utilization", memoryUtilization},
//                 {"threshold", threshold}
//             };
//             logger->info("Storage threshold exceeded", info, "THRESHOLD_EXCEEDED", false, "");
//         }

//         return memoryUtilization > threshold;

//     } catch (const std::exception& e) {
//         nlohmann::json errInfo = { {"error_reason", e.what()} };
//         logger->critical("Failed to check archival policy", errInfo, "CHECK_POLICY_FAIL", true, e.what());
//         return false;
//     }
// }

// std::vector<std::string> ArchivalController::getAllFilePaths() {
//     std::vector<std::string> paths;
//     try {
//         if (archivalPolicy.contains("VIDEO_RETENTION_POLICY_PATH")) {
//             paths.push_back(archivalPolicy["VIDEO_RETENTION_POLICY_PATH"].get<std::string>());
//         }
//         if (archivalPolicy.contains("PARQUET_RETENTION_POLICY_PATH")) {
//             paths.push_back(archivalPolicy["PARQUET_RETENTION_POLICY_PATH"].get<std::string>());
//         }
//         if (archivalPolicy.contains("DIAGNOSTIC_RETENTION_POLICY_PATH")) {
//             paths.push_back(archivalPolicy["DIAGNOSTIC_RETENTION_POLICY_PATH"].get<std::string>());
//         }
//         if (archivalPolicy.contains("LOG_RETENTION_POLICY_PATH")) {
//             paths.push_back(archivalPolicy["LOG_RETENTION_POLICY_PATH"].get<std::string>());
//         }
//         if (archivalPolicy.contains("VIDEO_CLIPS_RETENTION_POLICY_PATH")) {
//             paths.push_back(archivalPolicy["VIDEO_CLIPS_RETENTION_POLICY_PATH"].get<std::string>());
//         }
//         return paths;
//     } catch (const nlohmann::json::exception& e) {
//         nlohmann::json errInfo = { {"error_reason", e.what()} };
//         logger->critical("Failed to get file paths", errInfo, "GET_PATHS_FAIL", true, e.what());
//         throw;
//     }
// }

// double ArchivalController::diskSpaceUtilization() {
//     try {
//         if (!archivalPolicy.contains("MOUNTED_PATH")) {
//             nlohmann::json errInfo = { {"error_reason", "MOUNTED_PATH not found in archival policy"} };
//             logger->critical("MOUNTED_PATH not found in policy", errInfo, "MOUNTED_PATH_ERR", true, "MOUNTED_PATH key missing");
//             throw std::runtime_error("MOUNTED_PATH key not found in archival policy");
//         }

//         std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
//         uint64_t total = 0, used = 0, free = 0;

//         std::tie(total, used, free) = fileService.get_memory_details(mountedPath);

//         if (total == 0) {
//             nlohmann::json errInfo = { {"error_reason", "Total disk space is 0"} };
//             logger->critical("Invalid disk space information", errInfo, "DISK_INFO_ERR", true, "Total space is 0");
//             throw std::runtime_error("Invalid disk space information: total space is 0");
//         }

//         return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;

//     } catch (const std::exception& e) {
//         nlohmann::json errInfo = { {"error_reason", e.what()} };
//         logger->critical("Failed to get disk space utilization", errInfo, "DISK_UTIL_FAIL", true, e.what());
//         throw;
//     }
// }

// double ArchivalController::checkFileArchivalPolicy(const std::string& filePath) {
//     return fileService.get_file_age_in_hours(filePath);
// }

// bool ArchivalController::isFileEligibleForArchival(const std::string& filePath) {
//     const std::string jsonFilePath = "../configuration/archival_config.json";
//     std::ifstream file(jsonFilePath);
//     if (!file.is_open()) {
//         nlohmann::json errInfo = { {"error_reason", "Could not open file: " + jsonFilePath} };
//         logger->critical("Failed to open archival config", errInfo, "OPEN_CONFIG_FAIL", true, "Could not open archival_config.json");
//         throw std::runtime_error("Could not open archival_config.json");
//     }

//     nlohmann::json data;
//     try {
//         file >> data;
//         if (filePath.find("Videos") != std::string::npos) {
//             return data.value("VideosArchival", false);
//         } else if (filePath.find("Analysis") != std::string::npos) {
//             return data.value("AnalysisArchival", false);
//         } else if (filePath.find("Diagnostics") != std::string::npos) {
//             return data.value("DiagnosticsArchival", false);
//         } else if (filePath.find("Logs") != std::string::npos) {
//             return data.value("LogsArchival", false);
//         } else if (filePath.find("VideoClips") != std::string::npos) {
//             return data.value("VideoclipsArchival", false);
//         }
//     } catch (const nlohmann::json::exception& e) {
//         nlohmann::json errInfo = { {"error_reason", e.what()} };
//         logger->critical("Failed to parse archival config", errInfo, "PARSE_CONFIG_FAIL", true, e.what());
//         throw;
//     }

//     return false;
// }

// bool ArchivalController::isFileEligibleForDeletion(const std::string& filePath) {
//     double fileAge = checkFileArchivalPolicy(filePath);
//     double retentionPeriod;

//     if (filePath.find("Videos") != std::string::npos) {
//         retentionPeriod = vecowretentionpolicy::VIDEO_RETENTION_POLICY.retentionPeriodInHours;
//     } else if (filePath.find("Analysis") != std::string::npos) {
//         retentionPeriod = vecowretentionpolicy::PARQUET_RETENTION_POLICY.retentionPeriodInHours;
//     } else if (filePath.find("Diagnostics") != std::string::npos) {
//         retentionPeriod = vecowretentionpolicy::DIAGNOSTIC_RETENTION_POLICY.retentionPeriodInHours;
//     } else if (filePath.find("Logs") != std::string::npos) {
//         retentionPeriod = vecowretentionpolicy::LOG_RETENTION_POLICY.retentionPeriodInHours;
//     } else if (filePath.find("VideoClips") != std::string::npos) {
//         retentionPeriod = vecowretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY.retentionPeriodInHours;
//     } else {
//         return false;
//     }

//     return fileAge > retentionPeriod;
// }

// bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
//     auto query = "SELECT dds_video_file_location FROM analytics WHERE video_file_location = '" + filePath + "'";
//     return false;
// }

// void ArchivalController::updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath) {
//     auto query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath + "' WHERE video_file_location = '" + filePath + "'";
// }

// std::string ArchivalController::getDestinationPath(const std::string& filePath) {
//     auto destinationPath = filePath;
//     std::string mountedPath = archivalPolicy.at("MOUNTED_PATH").get<std::string>();
//     size_t pos = filePath.find(mountedPath);
    
//     if (pos != std::string::npos) {
//         destinationPath.replace(
//             pos,
//             mountedPath.length(),
//             archivalPolicy.at("DDS_PATH").get<std::string>()
//         );
//     } else {
//         nlohmann::json errInfo = { {"error_reason", "MOUNTED_PATH not found in filePath"} };
//         logger->critical("Failed to create destination path", errInfo, "DEST_PATH_ERR", true, "MOUNTED_PATH not found in filePath");
//     }
//     return destinationPath;
// }

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

FileService fileService;


  // Ensure this is included

DatabaseUtilities& db = DatabaseUtilities::getInstance(
    DatabaseUtilities::ConnectionParams{
        "localhost",          // Host
        "postgres",           // Username
        "mysecretpassword",   // Password
        "esk_galileo",        // Database name
        5432                  // Port
    }
);




ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                         const std::string& source,
                                         const std::string& logFilePath)
    : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
    try {

        logger = LoggingService::getInstance(source, logFilePath);
        // Log initialization started with a timestamp included
        logger->info("ArchivalController initialization started", 
                     createLogInfo({ {"message", "Initialization started successfully"} }),
                     "ARCH_INIT_START", false);

        if (!archivalPolicy.is_object()) {
            nlohmann::json errInfo = createLogInfo({ {"error_reason", "Archival policy is not a valid JSON object"} });
            logger->critical("Invalid archival policy: not a JSON object", 
                 errInfo, 
                 "ARCH_POLICY_INVALID", true, "");

            logIncidentToDB("Invalid archival policy: not a JSON object", 
                errInfo, 
                "ARCH_POLICY_INVALID");

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
        
        this->archivalPolicy = nlohmann::json(archivalPolicy);
        
    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"error_reason", e.what()} });
        logger->critical("Initialization failed", errInfo, "ARCH_INIT_FAIL", true, "");
         logIncidentToDB("Initialization failed", errInfo, "ARCH_INIT_FAIL");

        throw;
    }
}


// Function to log only critical, warning, and error incidents to the database
// void ArchivalController::logIncidentToDB(const std::string& message, const nlohmann::json& details, const std::string& error_code) {
//     try {
//         std::ostringstream query;
//         query << "INSERT INTO incidents (process_name, incident_message, incident_time, incident_details, recovery_status) VALUES ("
//               << "'" <<  << "', '" << message << "', NOW(), '" << details.dump() << "', 'PENDING') RETURNING id";  

//         int lastInsertId = db.executeInsert(query.str());  // Executes the query and fetches the ID

//         std::cout << "Inserted incident with ID: " << lastInsertId << std::endl;
        
//     } catch (const std::exception& e) {
//         std::cerr << "Database Insert Failed: " << e.what() << std::endl;
//         logger->error("Database Insert Failed", {{"error", e.what()}}, "DB_INSERT_FAIL", true, "05002");
//     }
// }
void ArchivalController::logIncidentToDB(const std::string& message, const nlohmann::json& details, const std::string& error_code) {
    try {
        std::ostringstream query;
        // Include the process name in the query. We assume your table has a column called process_name.
        // Here "EFMS" is hardcoded. You could also pass it as a parameter or retrieve it from a configuration.
        query << "INSERT INTO incidents (incident_message, incident_time, incident_details, process_name, recovery_status) VALUES ("
              << "'" << message << "', NOW(), '" << details.dump() << "', 'EFMS', 'PENDING') RETURNING id";

        int lastInsertId = db.executeInsert(query.str());  // Executes the query and fetches the ID

        std::cout << "Inserted incident with ID: " << lastInsertId << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Database Insert Failed: " << e.what() << std::endl;
        logger->error("Database Insert Failed", {{"error", e.what()}}, "DB_INSERT_FAIL", true, "05002");
    }
}



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
        auto [files, directories] = fileService.read_directory_recursively(filePath);

        for (const auto& file : files) {
            if (isFileEligibleForArchival(file)) {
                // if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
                //     nlohmann::json errInfo = createLogInfo({ {"error_reason", "DDS path not accessible: " + archivalPolicy.at("DDS_PATH").get<std::string>()} });
                //     logger->critical("DDS path not accessible", errInfo, "DDS_PATH_ERR", true, "05001");
                //     return;
                // }
                if (!fileService.is_mounted_drive_accessible(archivalPolicy.at("DDS_PATH"))) {
                        nlohmann::json errInfo = createLogInfo({ {"error_reason", "DDS path not accessible: " + archivalPolicy.at("DDS_PATH").get<std::string>()} });
                        // Log to file
                        logger->error("DDS path not accessible", errInfo, "DDS_PATH_ERR", true, "05001");
                        // Also log to the database
                        logIncidentToDB("DDS path not accessible", errInfo, "DDS_PATH_ERR");
                        return;
                    }


                auto destinationPath = getDestinationPath(file);
                if (!isFileArchivedToDDS(file)) {
                    nlohmann::json info = createLogInfo({ {"destination", destinationPath} });
                    logger->info("Archiving file", info, "FILE_ARCHIVE", false);
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
            nlohmann::json info = createLogInfo({ {"memory_utilization", memoryUtilization}});
            logger->critical("Missing Threshold Configuration", info, "THRESHOLD_EXCEEDED", false,"");
            return false;
        }

        const auto& thresholdStr = static_cast<std::string>(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"]);
        int threshold = std::stoi(thresholdStr);

        if (memoryUtilization > threshold) {
            nlohmann::json info = createLogInfo({ {"memory_utilization", memoryUtilization}, {"threshold", threshold} });
            logger->info("Storage threshold exceeded", info, "THRESHOLD_EXCEEDED", false);
        }

        return memoryUtilization > threshold;

    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"error_reason", e.what()} });
        logger->error("Failed to check archival policy", errInfo, "CHECK_POLICY_FAIL", true, "");
        logIncidentToDB("Failed to check archival policy", errInfo, "CHECK_POLICY_FAIL");

        return false;
    }
}

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
        nlohmann::json errInfo = createLogInfo({ {"error_reason", e.what()} });
        logger->critical("Failed to get file paths", errInfo, "GET_PATHS_FAIL", true, "");
        throw;
    }
}

double ArchivalController::diskSpaceUtilization() {
    try {
        if (!archivalPolicy.contains("MOUNTED_PATH")) {
            throw std::runtime_error("MOUNTED_PATH key not found in archival policy");
        }

        std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
        uint64_t total = 0, used = 0, free = 0;
        std::tie(total, used, free) = fileService.get_memory_details(mountedPath);

        if (total == 0) {
            nlohmann::json errInfo = createLogInfo({ {"error_reason", "Total disk space is 0"} });
           // Invalid Disk Space Info
           logger->critical("Invalid disk space information", errInfo, "DISK_INFO_ERR", true, "");
           logIncidentToDB("Invalid disk space information", errInfo, "DISK_INFO_ERR");

            throw std::runtime_error("Invalid disk space information: total space is 0");
        }

        return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;

    } catch (const std::exception& e) {
        nlohmann::json errInfo = createLogInfo({ {"error_reason", e.what()} });
        logger->critical("Failed to get disk space utilization", errInfo, "DISK_UTIL_FAIL", true, "");
        logIncidentToDB("Failed to get disk space utilization", errInfo, "DISK_UTIL_FAIL");
        throw;
    }
}

double ArchivalController::checkFileArchivalPolicy(const std::string& filePath) {
    return fileService.get_file_age_in_hours(filePath);
}

bool ArchivalController::isFileEligibleForArchival(const std::string& filePath) {
    const std::string jsonFilePath = "../configuration/archival_config.json";
    std::ifstream file(jsonFilePath);

    if (!file.is_open()) {
        nlohmann::json errInfo = createLogInfo({ {"error_reason",""} });
        logger->warning("Could not open archival_config.json",errInfo, "", true, "");
        logIncidentToDB("Could not open archival_config.json", errInfo, "DISK_UTIL_FAIL");
        throw std::runtime_error("Could not open archival_config.json");
    }

    nlohmann::json data;
    try {
        file >> data;
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
        nlohmann::json errInfo = createLogInfo({ {"error_reason", e.what()} });
        logger->critical("Failed to parse archival config", errInfo, "PARSE_CONFIG_FAIL", true, "");
        logIncidentToDB("Failed to parse archival config", errInfo, "PARSE_CONFIG_FAIL");

        throw;
    }

    return false;
}

bool ArchivalController::isFileEligibleForDeletion(const std::string& filePath) {
    double fileAge = checkFileArchivalPolicy(filePath);
    double retentionPeriod;

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

    return fileAge > retentionPeriod;
}

bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
    auto query = "SELECT dds_video_file_location FROM analytics WHERE video_file_location = '" + filePath + "'";
    return false;
}

void ArchivalController::updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath) {
    auto query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath + "' WHERE video_file_location = '" + filePath + "'";
}

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
        nlohmann::json errInfo = createLogInfo({ {"error_reason", "MOUNTED_PATH not found in filePath"} });
        logger->critical("Failed to create destination path", errInfo, "DEST_PATH_ERR", true, "");
        logIncidentToDB("Failed to create destination path", errInfo, "DEST_PATH_ERR");
    }
    return destinationPath;
}
