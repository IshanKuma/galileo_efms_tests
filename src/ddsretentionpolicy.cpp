#include "ddsretentionpolicy.hpp"
#include <ctime>
#include <filesystem>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

// Remove const from declarations in header file and initialize from config
std::int64_t ddsretentionpolicy::THRESHOLD_STORAGE_UTILIZATION;
std::string ddsretentionpolicy::DDS_PATH;
std::string ddsretentionpolicy::SPATIAL_PATH;
bool ddsretentionpolicy::IS_RETENTION_POLICY_ENABLED;
int ddsretentionpolicy::RETENTION_PERIOD_IN_HOURS;

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::DIAGNOSTIC_RETENTION_POLICY;
ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::LOG_RETENTION_POLICY;
ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY;

std::unordered_map<std::string, ddsretentionpolicy::RetentionPolicy> ddsretentionpolicy::VIDEO_STATION_POLICIES;
std::unordered_map<std::string, ddsretentionpolicy::RetentionPolicy> ddsretentionpolicy::ANALYSIS_STATION_POLICIES;

std::string ddsretentionpolicy::BASE_LOG_DIRECTORY;
std::string ddsretentionpolicy::LOG_DIRECTORY;
std::string ddsretentionpolicy::LOG_SOURCE;
std::string ddsretentionpolicy::LOG_FILE;
std::string ddsretentionpolicy::LOG_FILE_PATH;
std::string ddsretentionpolicy::PM;

static bool config_loaded = false;

void ddsretentionpolicy::loadConfig() {
    if (config_loaded) return;
    
    std::ifstream configFile("config.json");
    if (!configFile.is_open()) {
        throw std::runtime_error("Failed to open config.json");
    }
    
    try {
        nlohmann::json config;
        configFile >> config;
        
        auto ddsConfig = config["dds_retention_policy"];
        
        THRESHOLD_STORAGE_UTILIZATION = ddsConfig["threshold_storage_utilization"].get<int>();
        DDS_PATH = ddsConfig["dds_path"].get<std::string>();
        SPATIAL_PATH = ddsConfig["spatial_path"].get<std::string>();
        IS_RETENTION_POLICY_ENABLED = ddsConfig["is_retention_policy_enabled"].get<bool>();
        RETENTION_PERIOD_IN_HOURS = ddsConfig["retention_period_in_hours"].get<int>();
        BASE_LOG_DIRECTORY = ddsConfig["base_log_directory"].get<std::string>();
        LOG_SOURCE = ddsConfig["log_source"].get<std::string>();
        LOG_FILE = ddsConfig["log_file"].get<std::string>();
        PM = ddsConfig["pm"].get<std::string>();
        
        // Load retention policies
        auto retentionPolicies = ddsConfig["retention_policies"];
        
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
        auto stationPolicies = ddsConfig["station_policies"];
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

ddsretentionpolicy::ddsretentionpolicy() {
    loadConfig();
}

std::string ddsretentionpolicy::getCurrentDateFolder() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);
    return std::string(buffer);
}

std::string ddsretentionpolicy::getLogDirectory() {
    std::string logDir = BASE_LOG_DIRECTORY + getCurrentDateFolder() + "/";
    try {
        std::filesystem::create_directories(logDir);
    } catch (const std::filesystem::filesystem_error& e) {
        throw;
    }
    return logDir;
}

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> ddsretentionpolicy::to_dict() const {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;

    result["THRESHOLD_STORAGE_UTILIZATION"] = {{"value", std::to_string(THRESHOLD_STORAGE_UTILIZATION)}};
    result["DDS_PATH"] = {{"value", DDS_PATH}};
    result["SPATIAL_PATH"] = {{"value", SPATIAL_PATH}};
    result["IS_RETENTION_POLICY_ENABLED"] = {{"value", IS_RETENTION_POLICY_ENABLED ? "True" : "False"}};
    result["RETENTION_PERIOD_IN_HOURS"] = {{"value", std::to_string(RETENTION_PERIOD_IN_HOURS)}};

    // Include diagnostic, log, and video clips policies with retentionPeriod and enabled
    result["DIAGNOSTIC_RETENTION_POLICY_PATH"] = {
        {"value", DIAGNOSTIC_RETENTION_POLICY.path},
        {"retentionPeriod", std::to_string(DIAGNOSTIC_RETENTION_POLICY.retentionPeriod)},
        {"enabled", DIAGNOSTIC_RETENTION_POLICY.enabled ? "True" : "False"}
    };
    result["LOG_RETENTION_POLICY_PATH"] = {
        {"value", LOG_RETENTION_POLICY.path},
        {"retentionPeriod", std::to_string(LOG_RETENTION_POLICY.retentionPeriod)},
        {"enabled", LOG_RETENTION_POLICY.enabled ? "True" : "False"}
    };
    result["VIDEO_CLIPS_RETENTION_POLICY_PATH"] = {
        {"value", VIDEO_CLIPS_RETENTION_POLICY.path},
        {"retentionPeriod", std::to_string(VIDEO_CLIPS_RETENTION_POLICY.retentionPeriod)},
        {"enabled", VIDEO_CLIPS_RETENTION_POLICY.enabled ? "True" : "False"}
    };

    result["LOG_DIRECTORY"] = {{"value", LOG_DIRECTORY}};
    result["LOG_SOURCE"] = {{"value", LOG_SOURCE}};
    result["LOG_FILE"] = {{"value", LOG_FILE}};
    result["LOG_FILE_PATH"] = {{"value", LOG_FILE_PATH}};

    // Include station-specific video and analysis policies
    for (const auto& [station, policy] : VIDEO_STATION_POLICIES) {
        result["VIDEO_RETENTION_POLICY_" + station] = {
            {"value", policy.path},
            {"retentionPeriod", std::to_string(policy.retentionPeriod)},
            {"enabled", policy.enabled ? "True" : "False"}
        };
    }
    for (const auto& [station, policy] : ANALYSIS_STATION_POLICIES) {
        result["ANALYSIS_RETENTION_POLICY_" + station] = {
            {"value", policy.path},
            {"retentionPeriod", std::to_string(policy.retentionPeriod)},
            {"enabled", policy.enabled ? "True" : "False"}
        };
    }

    return result;
}