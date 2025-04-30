#ifndef VECOWRETENTIONPOLICY_HPP
#define VECOWRETENTIONPOLICY_HPP

#include <string>
#include <vector>
#include <unordered_map>

class vecowretentionpolicy {
public:
    // Constructor
    vecowretentionpolicy();

    // Data retrieval
    std::unordered_map<std::string, std::string> to_dict() const;

    // Utilities
    static std::string getCurrentDateFolder();
    static std::string getLogDirectory();

    // Constants
    static const std::int64_t THRESHOLD_STORAGE_UTILIZATION;
    static const std::string MOUNTED_PATH;
    static const std::string DDS_PATH;
    static const std::string SPATIAL_PATH;
    static const bool IS_RETENTION_POLICY_ENABLED;
    static const int RETENTION_PERIOD_IN_HOURS;

    struct RetentionPolicy {
        std::string path;
        bool isEnabled;
        int retentionPeriodInHours;
        std::vector<std::string> fileExtensions;
    };

    // Retention policies
    // static RetentionPolicy VIDEO_RETENTION_POLICY;
    // static RetentionPolicy PARQUET_RETENTION_POLICY;
    static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
    static RetentionPolicy LOG_RETENTION_POLICY;
    static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;

    // Log metadata
    static const std::string BASE_LOG_DIRECTORY;
    static const std::string LOG_DIRECTORY;
    static const std::string LOG_SOURCE;
    static const std::string LOG_FILE;
    static const std::string LOG_FILE_PATH;

    // Station-specific retention policies
    static std::unordered_map<std::string, RetentionPolicy> VIDEO_STATION_POLICIES;
    static std::unordered_map<std::string, RetentionPolicy> ANALYSIS_STATION_POLICIES;

private:
    static const std::string PM;
    // static void initialize_station_policies();  // Ensures static map setup
};

#endif // VECOWRETENTIONPOLICY_HPP
