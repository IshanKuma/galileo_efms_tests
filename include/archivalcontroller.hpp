#ifndef ARCHIVALCONTROLLER_H
#define ARCHIVALCONTROLLER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "fileservice.hpp"
#include "loggingservice.hpp"

// ArchivalController class declaration
class ArchivalController {
public:
    // Constructor
    ArchivalController(const nlohmann::json& archivalPolicy, const std::string &source, const std::string &logFilePath);

    // Public methods
    void applyArchivalPolicy();
    void startMaxUtilizationPipeline();
    void startNormalPipeline();
    void stopPipeline(const std::vector<std::string>& directories);
    FileService fileService;
    
    // logger = LoggingService::getInstance(source, logFilePath);
private:
    // Member variables
    nlohmann::json archivalPolicy;
    LoggingService* logger;
    std::string source;
    std::string logFilePath;
    // // CustomLogger customLogger;
    // DatabaseService databaseService;

    // Private helper methods
    bool checkArchivalPolicy();
    std::vector<std::string> getAllFilePaths();
    double diskSpaceUtilization();
    double checkFileArchivalPolicy(const std::string& filePath);
    bool isFileEligibleForArchival(const std::string& filePath);
    bool isFileEligibleForDeletion(const std::string& filePath);
    bool isFileArchivedToDDS(const std::string& filePath);
    void updateFileArchivalStatus(const std::string& filePath, const std::string& ddsFilePath);
    std::string getDestinationPath(const std::string& filePath);
};

#endif // ARCHIVALCONTROLLER_H
