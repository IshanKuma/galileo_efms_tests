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

    // Static variables (no longer const - will be loaded from config)
    static std::int64_t THRESHOLD_STORAGE_UTILIZATION;
    static std::string MOUNTED_PATH;
    static std::string DDS_PATH;
    static std::string SPATIAL_PATH;
    static bool IS_RETENTION_POLICY_ENABLED;
    static int RETENTION_PERIOD_IN_HOURS;

    struct RetentionPolicy {
        std::string path;
        bool isEnabled;
        int retentionPeriodInHours;
        std::vector<std::string> fileExtensions;
    };

    // Retention policies
    static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
    static RetentionPolicy LOG_RETENTION_POLICY;
    static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;

    // Log metadata
    static std::string BASE_LOG_DIRECTORY;
    static std::string LOG_DIRECTORY;
    static std::string LOG_SOURCE;
    static std::string LOG_FILE;
    static std::string LOG_FILE_PATH;

    // Station-specific retention policies
    static std::unordered_map<std::string, RetentionPolicy> VIDEO_STATION_POLICIES;
    static std::unordered_map<std::string, RetentionPolicy> ANALYSIS_STATION_POLICIES;

    static std::string PM;

    // Configuration loading
    static void loadConfig();
};

#endif // VECOWRETENTIONPOLICY_HPP