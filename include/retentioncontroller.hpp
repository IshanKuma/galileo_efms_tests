#ifndef RETENTIONCONTROLLER_H
#define RETENTIONCONTROLLER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "fileservice.hpp"
#include "loggingservice.hpp"

class RetentionController {
public:
    // Constructor
    RetentionController(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& retentionPolicy,
                        const std::string& logFilePath, 
                        const std::string& source);

    // Public methods
    void applyRetentionPolicy();
    void startMaxUtilizationPipeline();
    void startNormalPipeline();
    void stopPipeline(const std::vector<std::string>& directories);

private:
    // Member variables
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> retentionPolicy;
    FileService fileService;
    LoggingService* logger;
    std::string source;
    std::string logFilePath;
    // Private methods
    // bool validatePolicy();
    // bool validatePolicy(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& policies); // With arguments

    bool checkRetentionPolicy();
    std::vector<std::string> getAllFilePaths();
    double diskSpaceUtilization();
    bool checkFileRetentionPolicy(const std::string& filePath);
    bool checkFilePermissions(const std::string& filePath);
    bool isFileEligibleForDeletion(const std::string& filePath);
};

#endif // RETENTIONCONTROLLER_H
