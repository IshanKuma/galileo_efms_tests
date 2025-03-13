#include "vecowretentionpolicy.hpp"

#include <unordered_map>

// const int vecowretentionpolicy::THRESHOLD_STORAGE_UTILIZATION = 75;
const std::int64_t vecowretentionpolicy::THRESHOLD_STORAGE_UTILIZATION = 75;  // Or use std::int64_t

// const std::string vecowretentionpolicy::MOUNTED_PATH = "/mnt/storage";
// const std::string vecowretentionpolicy::DDS_PATH = "/mnt/dds/d";

const std::string vecowretentionpolicy::MOUNTED_PATH = "/mnt/sorage";
const std::string vecowretentionpolicy::DDS_PATH = "/mnt/dds/d";
const std::string vecowretentionpolicy::SPATIAL_PATH = MOUNTED_PATH + "/Lam/Data/" + "PMX" + "/Spatial";
const bool vecowretentionpolicy::IS_RETENTION_POLICY_ENABLED = true;
const int vecowretentionpolicy::RETENTION_PERIOD_IN_HOURS = 10 * 4;

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::VIDEO_RETENTION_POLICY = {
    SPATIAL_PATH + "/Videos", true, 24*4, {"mp4", "mkv"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::PARQUET_RETENTION_POLICY = {
    SPATIAL_PATH + "/Analysis", true, 24 * 90, {"parquet"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::DIAGNOSTIC_RETENTION_POLICY = {
    SPATIAL_PATH + "/Diagnostics", true, 24 * 30, {"csv", "json", "txt"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::LOG_RETENTION_POLICY = {
    SPATIAL_PATH + "/Logs", true, 24 * 90, {"log"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY = {
    SPATIAL_PATH + "/VideoClips", true, 24 * 90, {"mp4", "mkv"}
};

const std::string vecowretentionpolicy::LOG_DIRECTORY = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/General/ESK_EFMS";
const std::string vecowretentionpolicy::LOG_SOURCE = "EFMS";
const std::string vecowretentionpolicy::LOG_FILE = "VECOW_Retention_Archival.log";
const std::string vecowretentionpolicy::LOG_FILE_PATH = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/General/ESK_EFMS/";

const std::string vecowretentionpolicy::PM = "PMX";

vecowretentionpolicy::vecowretentionpolicy() {
    // Constructor implementation if necessary
}

std::unordered_map<std::string, std::string> vecowretentionpolicy::to_dict() const {
    std::unordered_map<std::string, std::string> result;
    
    result["THRESHOLD_STORAGE_UTILIZATION"] = std::to_string(THRESHOLD_STORAGE_UTILIZATION);
    result["MOUNTED_PATH"] = MOUNTED_PATH;
    result["DDS_PATH"] = DDS_PATH;
    result["SPATIAL_PATH"] = SPATIAL_PATH;
    result["IS_RETENTION_POLICY_ENABLED"] = IS_RETENTION_POLICY_ENABLED ? "True" : "False";
    result["RETENTION_PERIOD_IN_HOURS"] = std::to_string(RETENTION_PERIOD_IN_HOURS);
    result["VIDEO_RETENTION_POLICY_PATH"] = VIDEO_RETENTION_POLICY.path;
    result["PARQUET_RETENTION_POLICY_PATH"] = PARQUET_RETENTION_POLICY.path;
    result["DIAGNOSTIC_RETENTION_POLICY_PATH"] = DIAGNOSTIC_RETENTION_POLICY.path;
    result["LOG_RETENTION_POLICY_PATH"] = LOG_RETENTION_POLICY.path;
    result["VIDEO_CLIPS_RETENTION_POLICY_PATH"] = VIDEO_CLIPS_RETENTION_POLICY.path;
    result["LOG_DIRECTORY"] = LOG_DIRECTORY;
    result["LOG_SOURCE"] = LOG_SOURCE;
    result["LOG_FILE"] = LOG_FILE;
    result["LOG_FILE_PATH"] = LOG_FILE_PATH;

    return result;
}
