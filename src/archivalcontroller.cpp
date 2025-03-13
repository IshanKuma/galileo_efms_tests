#include "archivalcontroller.hpp"
#include "fileservice.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <tuple>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <string>
#include <vecowretentionpolicy.hpp>

FileService fileService;

ArchivalController::ArchivalController(const nlohmann::json& archivalPolicy,
                                     const std::string& source,
                                     const std::string& logFilePath)
    : archivalPolicy(archivalPolicy), source(source), logFilePath(logFilePath) {
    try {
        logger = LoggingService::getInstance(source, logFilePath);
        
        logger->info("ArchivalController initialization started", nlohmann::json{{"ArchivalController started"}}, "EFMS");
        
        if (!archivalPolicy.is_object()) {
            logger->critical("Invalid archival policy: not a JSON object", nlohmann::json{{"invalid json object"}}, "EFMS", true, "05001");
            throw std::runtime_error("Invalid archival policy: not a JSON object");
        }

        const std::vector<std::pair<std::string, nlohmann::json::value_t>> requiredFields = {
            {"MOUNTED_PATH", nlohmann::json::value_t::string},
            {"DDS_PATH", nlohmann::json::value_t::string},
            {"THRESHOLD_STORAGE_UTILIZATION", nlohmann::json::value_t::string}
        };

        for (const auto& [field, expected_type] : requiredFields) {
            if (!archivalPolicy.contains(field)) {
                logger->critical("Missing required field: " + field, nlohmann::json{{"Required fileds missing"}}, "EFMS", true, "05001");
                throw std::runtime_error("Missing required field in archival policy: " + field);
            }
        }
        
        this->archivalPolicy = nlohmann::json(archivalPolicy);
        
    } catch (const std::exception& e) {
        logger->critical("Initialization failed", nlohmann::json{{"exception", e.what()}}, "EFMS", true, "05001");
        throw;
    }
}

void ArchivalController::applyArchivalPolicy() {
    if (checkArchivalPolicy()) {
        logger->info("Starting max utilization pipeline", nlohmann::json{{"status", "Storage threshold exceeded"}}, "EFMS");
        startMaxUtilizationPipeline();
    } else {
        startNormalPipeline();
        logger->info("Starting Normal utilization pipeline", nlohmann::json{{"Starting the normal pipeline"}}, "EFMS");
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
                    logger->critical("DDS path not accessible", 
                        nlohmann::json{{"DDS_PATH", archivalPolicy.at("DDS_PATH").get<std::string>()}},
                        "EFMS", false, "05001");
                    return;
                }

                auto destinationPath = getDestinationPath(file);
                if (!isFileArchivedToDDS(file)) {
                    logger->info("Archiving file", 
                        nlohmann::json{{"sourceFile", file}, {"destination", destinationPath}},
                        "EFMS", false, "0");
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
            logger->critical("Missing threshold configuration", nlohmann::json::object(), "EFMS", false, "05001");
            return false;
        }

        const auto& thresholdStr = static_cast<std::string>(archivalPolicy["THRESHOLD_STORAGE_UTILIZATION"]);
        int threshold = std::stoi(thresholdStr);

        if (memoryUtilization > threshold) {
            logger->info("Storage threshold exceeded", 
                nlohmann::json{{"Utilization", memoryUtilization}, {"Threshold", threshold}},
                "EFMS", false, "0");
        }

        return memoryUtilization > threshold;

    } catch (const std::exception& e) {
        logger->critical("Failed to check archival policy", nlohmann::json{{"exception", e.what()}}, "EFMS", false, "05001");
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
        logger->critical("Failed to get file paths", nlohmann::json{{"exception", e.what()}}, "EFMS", false, "05001");
        throw;
    }
}

double ArchivalController::diskSpaceUtilization() {
    try {
        if (!archivalPolicy.contains("MOUNTED_PATH")) {
            logger->critical("MOUNTED_PATH not found in policy", nlohmann::json::object(), "EFMS", false, "05001");
            throw std::runtime_error("MOUNTED_PATH key not found in archival policy");
        }

        std::string mountedPath = archivalPolicy["MOUNTED_PATH"].get<std::string>();
        uint64_t total = 0, used = 0, free = 0;

        std::tie(total, used, free) = fileService.get_memory_details(mountedPath);

        if (total == 0) {
            logger->critical("Invalid disk space information", nlohmann::json{{"Total", total}}, "EFMS", false, "05001");
            throw std::runtime_error("Invalid disk space information: total space is 0");
        }

        return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;

    } catch (const std::exception& e) {
        logger->critical("Failed to get disk space utilization", nlohmann::json{{"exception", e.what()}}, "EFMS", false, "05001");
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
        logger->critical("Failed to open archival config", nlohmann::json{{"path", jsonFilePath}}, "EFMS", false, "05001");
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
        logger->critical("Failed to parse archival config", nlohmann::json{{"exception", e.what()}}, "EFMS", false, "05001");
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
    auto query = "UPDATE analytics SET dds_video_file_location = '" + ddsFilePath +
                 "' WHERE video_file_location = '" + filePath + "'";
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
        logger->critical("Failed to create destination path", 
            nlohmann::json{{"error", "MOUNTED_PATH not found in filePath"}},
            "EFMS", false, "05001");
    }
    return destinationPath;
}
