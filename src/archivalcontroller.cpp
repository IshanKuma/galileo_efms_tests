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

DatabaseUtilities& db = DatabaseUtilities::getInstance({
    "localhost",          
    "postgres",           
    "mysecretpassword",   
    "esk_galileo",        
    5432                  
});

ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                       const std::string& source,
                                       const std::string& logFilePath)
    : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
    try {
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
        std::ostringstream query;
        query << "INSERT INTO incidents (incident_message, incident_time, incident_details, process_name, recovery_status) VALUES ('"
              << message << "', NOW(), '" << details.dump() << "', 'EFMS', 'PENDING') RETURNING id";
        int lastInsertId = db.executeInsert(query.str());
        std::cout << "Inserted incident with ID: " << lastInsertId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Database Insert Failed: " << e.what() << std::endl;
        logger->error("Database Insert Failed", {{"error", e.what()}}, "DB_INSERT_FAIL", true, "05003");
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
        if (!archivalPolicy.contains("RETENTION_POLICY_PATHS") || !archivalPolicy["RETENTION_POLICY_PATHS"].is_object()) {
            return paths;
        }
        const auto& retentionPaths = archivalPolicy["RETENTION_POLICY_PATHS"];
        for (const auto& [key, value] : retentionPaths.items()) {
            if (value.is_string()) {
                paths.push_back(value.get<std::string>());
            }
        }
    } catch (const std::exception& e) {
        logger->critical("Failed to get file paths", createLogInfo({{"detail", e.what()}}), "GET_PATHS_FAIL", true, "05007");
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
    try {
        if (!archivalPolicy.contains("ARCHIVAL_FLAGS") || !archivalPolicy["ARCHIVAL_FLAGS"].is_object()) return false;
        const auto& flags = archivalPolicy["ARCHIVAL_FLAGS"];

        if (filePath.find("Videos") != std::string::npos && flags.contains("Videos")) {
            return flags["Videos"].get<bool>();
        } else if (filePath.find("Analysis") != std::string::npos && flags.contains("Analysis")) {
            return flags["Analysis"].get<bool>();
        } else if (filePath.find("Diagnostics") != std::string::npos && flags.contains("Diagnostics")) {
            return flags["Diagnostics"].get<bool>();
        } else if (filePath.find("Logs") != std::string::npos && flags.contains("Logs")) {
            return flags["Logs"].get<bool>();
        } else if (filePath.find("VideoClips") != std::string::npos && flags.contains("VideoClips")) {
            return flags["VideoClips"].get<bool>();
        }
    } catch (...) {}
    return false;
}

bool ArchivalController::isFileArchivedToDDS(const std::string& filePath) {
    std::string query;
    if (filePath.find("Videos") != std::string::npos) {
        query = "SELECT dds_video_file_location FROM analytics WHERE video_file_location = '" + filePath + "'";
    } else if (filePath.find("Analysis") != std::string::npos) {
        query = "SELECT dds_parquet_file_location FROM analytics WHERE parquet_file_location = '" + filePath + "'";
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

    auto result = db.executeSelect(query);
    for (const auto& row : result) {
        if (row.empty() || row[0].empty()) {
            return false;
        }
    }
    return !result.empty();
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
