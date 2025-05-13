#include "vecowretentionpolicy.hpp"
#include <ctime>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

// Remove const from declarations in header file and initialize from config
std::int64_t vecowretentionpolicy::THRESHOLD_STORAGE_UTILIZATION;
std::string vecowretentionpolicy::MOUNTED_PATH;
std::string vecowretentionpolicy::DDS_PATH;
std::string vecowretentionpolicy::SPATIAL_PATH;
bool vecowretentionpolicy::IS_RETENTION_POLICY_ENABLED;
int vecowretentionpolicy::RETENTION_PERIOD_IN_HOURS;

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::DIAGNOSTIC_RETENTION_POLICY;
vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::LOG_RETENTION_POLICY;
vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY;

std::unordered_map<std::string, vecowretentionpolicy::RetentionPolicy> vecowretentionpolicy::VIDEO_STATION_POLICIES;
std::unordered_map<std::string, vecowretentionpolicy::RetentionPolicy> vecowretentionpolicy::ANALYSIS_STATION_POLICIES;

std::string vecowretentionpolicy::BASE_LOG_DIRECTORY;
std::string vecowretentionpolicy::LOG_DIRECTORY;
std::string vecowretentionpolicy::LOG_SOURCE;
std::string vecowretentionpolicy::LOG_FILE;
std::string vecowretentionpolicy::LOG_FILE_PATH;
std::string vecowretentionpolicy::PM;

static bool config_loaded = false;

void vecowretentionpolicy::loadConfig() {
    if (config_loaded) return;
    
    std::ifstream configFile("../configuration/config.json");
    if (!configFile.is_open()) {
        throw std::runtime_error("Failed to open config.json");
    }
    
    try {
        nlohmann::json config;
        configFile >> config;
        
        auto vecowConfig = config["vecow_retention_policy"];
        
        THRESHOLD_STORAGE_UTILIZATION = vecowConfig["threshold_storage_utilization"].get<int>();
        MOUNTED_PATH = vecowConfig["mounted_path"].get<std::string>();
        DDS_PATH = vecowConfig["dds_path"].get<std::string>();
        SPATIAL_PATH = vecowConfig["spatial_path"].get<std::string>();
        IS_RETENTION_POLICY_ENABLED = vecowConfig["is_retention_policy_enabled"].get<bool>();
        RETENTION_PERIOD_IN_HOURS = vecowConfig["retention_period_in_hours"].get<int>();
        BASE_LOG_DIRECTORY = vecowConfig["base_log_directory"].get<std::string>();
        LOG_SOURCE = vecowConfig["log_source"].get<std::string>();
        LOG_FILE = vecowConfig["log_file"].get<std::string>();
        PM = vecowConfig["pm"].get<std::string>();
        
        // Load retention policies
        auto retentionPolicies = vecowConfig["retention_policies"];
        
        auto diagPolicy = retentionPolicies["diagnostic"];
        DIAGNOSTIC_RETENTION_POLICY = {
            diagPolicy["path"].get<std::string>(),
            diagPolicy["enabled"].get<bool>(),
            diagPolicy["retention_hours"].get<int>(),
            diagPolicy["file_types"].get<std::vector<std::string>>()
        };
        
        auto logPolicy = retentionPolicies["log"];
        LOG_RETENTION_POLICY = {
            logPolicy["path"].get<std::string>(),
            logPolicy["enabled"].get<bool>(),
            logPolicy["retention_hours"].get<int>(),
            logPolicy["file_types"].get<std::vector<std::string>>()
        };
        
        auto videoClipsPolicy = retentionPolicies["video_clips"];
        VIDEO_CLIPS_RETENTION_POLICY = {
            videoClipsPolicy["path"].get<std::string>(),
            videoClipsPolicy["enabled"].get<bool>(),
            videoClipsPolicy["retention_hours"].get<int>(),
            videoClipsPolicy["file_types"].get<std::vector<std::string>>()
        };
        
        // Load station policies
        auto stationPolicies = vecowConfig["station_policies"];
        std::vector<std::string> stations = stationPolicies["stations"].get<std::vector<std::string>>();
        
        auto videoPolicy = stationPolicies["video"];
        auto analysisPolicy = stationPolicies["analysis"];
        
        for (const auto& station : stations) {
            VIDEO_STATION_POLICIES[station] = {
                SPATIAL_PATH + "/" + station + videoPolicy["path_suffix"].get<std::string>(),
                videoPolicy["enabled"].get<bool>(),
                videoPolicy["retention_hours"].get<int>(),
                videoPolicy["file_types"].get<std::vector<std::string>>()
            };
            
            ANALYSIS_STATION_POLICIES[station] = {
                SPATIAL_PATH + "/" + station + analysisPolicy["path_suffix"].get<std::string>(),
                analysisPolicy["enabled"].get<bool>(),
                analysisPolicy["retention_hours"].get<int>(),
                analysisPolicy["file_types"].get<std::vector<std::string>>()
            };
        }
        
        // Set log directory
        LOG_DIRECTORY = getLogDirectory();
        LOG_FILE_PATH = LOG_DIRECTORY;
        
        config_loaded = true;
        
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse config.json: " + std::string(e.what()));
    }
    
    configFile.close();
}

vecowretentionpolicy::vecowretentionpolicy() {
    loadConfig();
}

std::string vecowretentionpolicy::getCurrentDateFolder() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);
    return std::string(buffer);
}

std::string vecowretentionpolicy::getLogDirectory() {
    std::string logDir = BASE_LOG_DIRECTORY + getCurrentDateFolder() + "/";
    try {
        std::filesystem::create_directories(logDir);
    } catch (const std::filesystem::filesystem_error& e) {
        throw;
    }
    return logDir;
}

std::unordered_map<std::string, std::string> vecowretentionpolicy::to_dict() const {
    std::unordered_map<std::string, std::string> result;

    result["THRESHOLD_STORAGE_UTILIZATION"] = std::to_string(THRESHOLD_STORAGE_UTILIZATION);
    result["MOUNTED_PATH"] = MOUNTED_PATH;
    result["DDS_PATH"] = DDS_PATH;
    result["SPATIAL_PATH"] = SPATIAL_PATH;
    result["IS_RETENTION_POLICY_ENABLED"] = IS_RETENTION_POLICY_ENABLED ? "True" : "False";
    result["RETENTION_PERIOD_IN_HOURS"] = std::to_string(RETENTION_PERIOD_IN_HOURS);

    result["DIAGNOSTIC_RETENTION_POLICY_PATH"] = DIAGNOSTIC_RETENTION_POLICY.path;
    result["LOG_RETENTION_POLICY_PATH"] = LOG_RETENTION_POLICY.path;
    result["VIDEO_CLIPS_RETENTION_POLICY_PATH"] = VIDEO_CLIPS_RETENTION_POLICY.path;
    result["LOG_DIRECTORY"] = LOG_DIRECTORY;
    result["LOG_SOURCE"] = LOG_SOURCE;
    result["LOG_FILE"] = LOG_FILE;
    result["LOG_FILE_PATH"] = LOG_FILE_PATH;

    for (const auto& [station, policy] : VIDEO_STATION_POLICIES) {
        result["VIDEO_RETENTION_POLICY_" + station] = policy.path;
    }
    for (const auto& [station, policy] : ANALYSIS_STATION_POLICIES) {
        result["ANALYSIS_RETENTION_POLICY_" + station] = policy.path;
    }

    return result;
}