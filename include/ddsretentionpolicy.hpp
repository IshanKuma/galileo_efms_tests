#ifndef DDSRETENTIONPOLICY_HPP
#define DDSRETENTIONPOLICY_HPP

#include <string>
#include <vector>
#include <unordered_map>

class PMService {
public:
    static std::string get_pmid() {
        return "PMX";  // Placeholder implementation
    }
};

class ddsretentionpolicy {
public:
    // Constructor
    ddsretentionpolicy();

    // Methods
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> to_dict() const;
    static std::string getCurrentDateFolder();
    static std::string getLogDirectory();

    // Data structure
    struct RetentionPolicy {
        std::string path;
        bool enabled;
        int retentionPeriod;
        std::vector<std::string> fileExtensions;
    };

    // Static variables (no longer const - will be loaded from config)
    static std::int64_t THRESHOLD_STORAGE_UTILIZATION;
    static std::string DDS_PATH;
    static std::string SPATIAL_PATH;
    static bool IS_RETENTION_POLICY_ENABLED;
    static int RETENTION_PERIOD_IN_HOURS;

    // Retention Policies
    static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
    static RetentionPolicy LOG_RETENTION_POLICY;
    static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;

    // Logging
    static std::string BASE_LOG_DIRECTORY;
    static std::string LOG_DIRECTORY;
    static std::string LOG_SOURCE;
    static std::string LOG_FILE;
    static std::string LOG_FILE_PATH;

    // Station Policies
    static std::unordered_map<std::string, RetentionPolicy> VIDEO_STATION_POLICIES;
    static std::unordered_map<std::string, RetentionPolicy> ANALYSIS_STATION_POLICIES;

    static std::string PM;

    // Configuration loading
    static void loadConfig();
};

#endif // DDSRETENTIONPOLICY_HPP