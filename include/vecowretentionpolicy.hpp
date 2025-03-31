#ifndef VECOWRETENTIONPOLICY_HPP
#define VECOWRETENTIONPOLICY_HPP

#include <string>
#include <vector>
#include <unordered_map>


// class vecowretentionpolicy {
// public:
//     // Constructor
//     vecowretentionpolicy();

//     // Methods to return data
//     std::unordered_map<std::string, std::string> to_dict() const;

//     // Class members
//     static const std::int64_t THRESHOLD_STORAGE_UTILIZATION;
//     static const std::string MOUNTED_PATH;
//     static const std::string DDS_PATH;
//     static const std::string SPATIAL_PATH;
//     static const bool IS_RETENTION_POLICY_ENABLED;
//     static const int RETENTION_PERIOD_IN_HOURS;
    
//     struct RetentionPolicy {
//         std::string path;
//         bool isEnabled;
//         int retentionPeriodInHours;
//         std::vector<std::string> fileExtensions;
//     };

//     static RetentionPolicy VIDEO_RETENTION_POLICY;
//     static RetentionPolicy PARQUET_RETENTION_POLICY;
//     static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
//     static RetentionPolicy LOG_RETENTION_POLICY;
//     static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;
    
//     static const std::string LOG_DIRECTORY;
//     static const std::string LOG_SOURCE;
//     static const std::string LOG_FILE;
//     static const std::string LOG_FILE_PATH;

// private:
//     static const std::string PM;
// };

// #endif // VECOWRETENTIONPOLICY_HPP
class vecowretentionpolicy {
public:
    // Constructor
    vecowretentionpolicy();
    
    // Methods to return data
    std::unordered_map<std::string, std::string> to_dict() const;
    
    // New method to get current date folder
    static std::string getCurrentDateFolder();
    
    // New method to get log directory
    static std::string getLogDirectory();

    // Class members
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
    
    static RetentionPolicy VIDEO_RETENTION_POLICY;
    static RetentionPolicy PARQUET_RETENTION_POLICY;
    static RetentionPolicy DIAGNOSTIC_RETENTION_POLICY;
    static RetentionPolicy LOG_RETENTION_POLICY;
    static RetentionPolicy VIDEO_CLIPS_RETENTION_POLICY;
    
    // Updated log-related constants
    static const std::string BASE_LOG_DIRECTORY;
    static const std::string LOG_DIRECTORY;
    static const std::string LOG_SOURCE;
    static const std::string LOG_FILE;
    static const std::string LOG_FILE_PATH;

private:
    static const std::string PM;
};

#endif // VECOWRETENTIONPOLICY_HPP