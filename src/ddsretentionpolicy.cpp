#include "ddsretentionpolicy.hpp"
#include <ctime>
#include <filesystem>
#include <unordered_map>

// const int ddsretentionpolicy::THRESHOLD_STORAGE_UTILIZATION = 85;
const std::int64_t ddsretentionpolicy::THRESHOLD_STORAGE_UTILIZATION = 85;  // Or use std::int64_t
const std::string ddsretentionpolicy::DDS_PATH = "/mnt/dds/d";
const std::string ddsretentionpolicy::SPATIAL_PATH = DDS_PATH + "/Lam/Data/" + "PMX" + "/Spatial";
const bool ddsretentionpolicy::IS_RETENTION_POLICY_ENABLED = true;
const int ddsretentionpolicy::RETENTION_PERIOD_IN_HOURS = 0;

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::VIDEO_RETENTION_POLICY = {
    SPATIAL_PATH + "/Videos", true, 24*4, {"mp4", "mkv"}
};

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::PARQUET_RETENTION_POLICY = {
    SPATIAL_PATH + "/Analysis", true, 24*4, {"parquet"}
};

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::DIAGNOSTIC_RETENTION_POLICY = {
    SPATIAL_PATH + "/Diagnostics", true, 24*4, {"csv", "json", "txt"}
};

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::LOG_RETENTION_POLICY = {
    SPATIAL_PATH + "/Logs", true, 24*4, {"log"}
};

ddsretentionpolicy::RetentionPolicy ddsretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY = {
    SPATIAL_PATH + "/VideoClips", true, 24*4, {"mp4", "mkv"}
};

std::string ddsretentionpolicy::getCurrentDateFolder() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    
    char buffer[11];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);
    
    return std::string(buffer);
}

std::string ddsretentionpolicy::getLogDirectory() {
    std::string logDir = BASE_LOG_DIRECTORY + getCurrentDateFolder() + "/";
    
    // Ensure directory exists
    try {
        std::filesystem::create_directories(logDir);
    } catch (const std::filesystem::filesystem_error& e) {
        throw;
    }
    
    return logDir;
}
const std::string ddsretentionpolicy::BASE_LOG_DIRECTORY = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/";
// const std::string ddsretentionpolicy::LOG_DIRECTORY = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/General/ESK_EFMS/";
const std::string ddsretentionpolicy::LOG_DIRECTORY = getLogDirectory();

const std::string ddsretentionpolicy::LOG_SOURCE = "EFMS";
const std::string ddsretentionpolicy::LOG_FILE = "DDS_Retention.log";
// const std::string ddsretentionpolicy::LOG_FILE_PATH = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/General/ESK_EFMS";
const std::string ddsretentionpolicy::LOG_FILE_PATH = LOG_DIRECTORY;

const std::string ddsretentionpolicy::PM = "PMX";

ddsretentionpolicy::ddsretentionpolicy() {
    // Constructor implementation if necessary
}

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> ddsretentionpolicy::to_dict() const {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;
    
    // Ensure that the numeric fields are stored as strings
    result["THRESHOLD_STORAGE_UTILIZATION"] = {{"value", std::to_string(THRESHOLD_STORAGE_UTILIZATION)}};
    result["DDS_PATH"] = {{"value", DDS_PATH}};
    result["SPATIAL_PATH"] = {{"value", SPATIAL_PATH}};
    result["IS_RETENTION_POLICY_ENABLED"] = {{"value", IS_RETENTION_POLICY_ENABLED ? "True" : "False"}};
    result["RETENTION_PERIOD_IN_HOURS"] = {{"value", std::to_string(RETENTION_PERIOD_IN_HOURS)}};
    
    result["VIDEO_RETENTION_POLICY_PATH"] = {{"value", VIDEO_RETENTION_POLICY.path}};
    result["PARQUET_RETENTION_POLICY_PATH"] = {{"value", PARQUET_RETENTION_POLICY.path}};
    result["DIAGNOSTIC_RETENTION_POLICY_PATH"] = {{"value", DIAGNOSTIC_RETENTION_POLICY.path}};
    result["LOG_RETENTION_POLICY_PATH"] = {{"value", LOG_RETENTION_POLICY.path}};
    result["VIDEO_CLIPS_RETENTION_POLICY_PATH"] = {{"value", VIDEO_CLIPS_RETENTION_POLICY.path}};
    result["LOG_DIRECTORY"] = {{"value", LOG_DIRECTORY}};
    result["LOG_SOURCE"] = {{"value", LOG_SOURCE}};
    result["LOG_FILE"] = {{"value", LOG_FILE}};
    result["LOG_FILE_PATH"] = {{"value", LOG_FILE_PATH}};
    
    return result;
}



