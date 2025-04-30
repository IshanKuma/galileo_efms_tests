// #ifndef DDSRETENTIONPOLICY_HPP
// #define DDSRETENTIONPOLICY_HPP

// #include <string>
// #include <vector>
// #include <unordered_map>

// class PMService {
// public:
//     static std::string get_pmid() {
//         return "PMX";  // Placeholder implementation
//     }
// };

// class ddsretentionpolicy {
// public:
//     // Constructor
//     ddsretentionpolicy();

//     // Methods
//     std::unordered_map<std::string, std::unordered_map<std::string, std::string>> to_dict() const;
//     static std::string getCurrentDateFolder();
//     static std::string getLogDirectory();

//     // Data structure
//     struct RetentionPolicy {
//         std::string path;
//         bool isEnabled;
//         int retentionPeriodInHours;
//         std::vector<std::string> fileExtensions;
//     };

//     // Constants
//     static const std::int64_t THRESHOLD_STORAGE_UTILIZATION;
//     static const std::string DDS_PATH;
//     static const std::string SPATIAL_PATH;
//     static const bool IS_RETENTION_POLICY_ENABLED;
//     static const int RETENTION_PERIOD_IN_HOURS;

//     // Retention Policies
//     static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
//     static RetentionPolicy LOG_RETENTION_POLICY;
//     static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;

//     // Logging
//     static const std::string BASE_LOG_DIRECTORY;
//     static const std::string LOG_DIRECTORY;
//     static const std::string LOG_SOURCE;
//     static const std::string LOG_FILE;
//     static const std::string LOG_FILE_PATH;

//     // Station Policies
//     static std::unordered_map<std::string, RetentionPolicy> VIDEO_STATION_POLICIES;
//     static std::unordered_map<std::string, RetentionPolicy> ANALYSIS_STATION_POLICIES;

// private:
//     static const std::string PM;
//     // static void initialize_station_policies();
// };

// #endif // DDSRETENTIONPOLICY_HPP
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

    // Constants
    static const std::int64_t THRESHOLD_STORAGE_UTILIZATION;
    static const std::string DDS_PATH;
    static const std::string SPATIAL_PATH;
    static const bool IS_RETENTION_POLICY_ENABLED;
    static const int RETENTION_PERIOD_IN_HOURS;

    // Retention Policies
    static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
    static RetentionPolicy LOG_RETENTION_POLICY;
    static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;

    // Logging
    static const std::string BASE_LOG_DIRECTORY;
    static const std::string LOG_DIRECTORY;
    static const std::string LOG_SOURCE;
    static const std::string LOG_FILE;
    static const std::string LOG_FILE_PATH;

    // Station Policies
    static std::unordered_map<std::string, RetentionPolicy> VIDEO_STATION_POLICIES;
    static std::unordered_map<std::string, RetentionPolicy> ANALYSIS_STATION_POLICIES;

private:
    static const std::string PM;
};

#endif // DDSRETENTIONPOLICY_HPP
