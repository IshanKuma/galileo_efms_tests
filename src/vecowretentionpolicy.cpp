#include "vecowretentionpolicy.hpp"
#include <ctime>
#include <filesystem>

const std::int64_t vecowretentionpolicy::THRESHOLD_STORAGE_UTILIZATION = 75;
const std::string vecowretentionpolicy::MOUNTED_PATH = "/mnt/storage";
const std::string vecowretentionpolicy::DDS_PATH = "/mnt/dds/d";
const std::string vecowretentionpolicy::SPATIAL_PATH = MOUNTED_PATH + "/Lam/Data/" + "PMX" + "/Spatial";
const bool vecowretentionpolicy::IS_RETENTION_POLICY_ENABLED = true;
const int vecowretentionpolicy::RETENTION_PERIOD_IN_HOURS = 10 * 4;

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::DIAGNOSTIC_RETENTION_POLICY = {
    SPATIAL_PATH + "/Diagnostics", true, 24 * 30, {"csv", "json", "txt"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::LOG_RETENTION_POLICY = {
    SPATIAL_PATH + "/Logs", true, 24 * 90, {"log"}
};

vecowretentionpolicy::RetentionPolicy vecowretentionpolicy::VIDEO_CLIPS_RETENTION_POLICY = {
    SPATIAL_PATH + "/VideoClips", true, 24 * 90, {"mp4", "mkv"}
};

std::unordered_map<std::string, vecowretentionpolicy::RetentionPolicy> vecowretentionpolicy::VIDEO_STATION_POLICIES = {};
std::unordered_map<std::string, vecowretentionpolicy::RetentionPolicy> vecowretentionpolicy::ANALYSIS_STATION_POLICIES = {};

const std::string vecowretentionpolicy::BASE_LOG_DIRECTORY = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/";
const std::string vecowretentionpolicy::LOG_DIRECTORY = vecowretentionpolicy::getLogDirectory();
const std::string vecowretentionpolicy::LOG_SOURCE = "EFMS";
const std::string vecowretentionpolicy::LOG_FILE = "VECOW_Retention_Archival.log";
const std::string vecowretentionpolicy::LOG_FILE_PATH = LOG_DIRECTORY;
const std::string vecowretentionpolicy::PM = "PMX";

vecowretentionpolicy::vecowretentionpolicy() {
    std::vector<std::string> stations = {"Station1", "Station2", "Station3", "Station4"};

    for (const auto& station : stations) {
        VIDEO_STATION_POLICIES[station] = {
            SPATIAL_PATH + "/" + station + "/Videos", true, 24 * 4, {"mp4", "mkv"}
        };
        ANALYSIS_STATION_POLICIES[station] = {
            SPATIAL_PATH + "/" + station + "/Analysis", true, 24 * 90, {"parquet"}
        };
    }
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
