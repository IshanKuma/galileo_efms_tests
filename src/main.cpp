#include <sys/prctl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include "ddsretentionpolicy.hpp"
#include "vecowretentionpolicy.hpp"
#include "archivalcontroller.hpp"
#include "retentioncontroller.hpp"
#include "logutils.hpp"
#include <thread>
#include <nlohmann/json.hpp>
#include <fstream>

class JobScheduler {
private:
    // Policy and controller objects
    vecowretentionpolicy vecow_retention_policy;
    ddsretentionpolicy dds_retention_policy;
    
    // Controllers
    ArchivalController archival_controller;
    RetentionController retention_controller;
    
    // Last execution times
    std::chrono::steady_clock::time_point last_archival_run;
    std::chrono::steady_clock::time_point last_retention_run;
    
    // Configuration values
    int archival_interval_minutes;
    int retention_interval_minutes;
    int poll_interval_seconds;
    
    void loadConfig() {
        std::ifstream configFile("../configuration/config.json");
        if (!configFile.is_open()) {
            throw std::runtime_error("Main - Failed to open config.json");
        }
        
        try {
            nlohmann::json config;
            configFile >> config;
            
            auto scheduler = config["scheduler"];
            archival_interval_minutes = scheduler["archival_interval_minutes"].get<int>();
            retention_interval_minutes = scheduler["retention_interval_minutes"].get<int>();
            poll_interval_seconds = scheduler["poll_interval_seconds"].get<int>();
            
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error("Failed to parse config.json: " + std::string(e.what()));
        }
        
        configFile.close();
    }

public:
    JobScheduler() : 
        archival_controller(
            vecow_retention_policy.to_dict(),
            vecow_retention_policy.LOG_SOURCE,
            vecow_retention_policy.LOG_FILE_PATH
        ),
        retention_controller(
            dds_retention_policy.to_dict(),
            dds_retention_policy.LOG_FILE_PATH,
            dds_retention_policy.LOG_SOURCE
        ),
        last_archival_run(std::chrono::steady_clock::now()),
        last_retention_run(std::chrono::steady_clock::now())
    {
        loadConfig();
    }

    void run() {
        while (true) {
            auto now = std::chrono::steady_clock::now();
            
            // Check if configured minutes have passed since last archival run
            auto archival_duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_archival_run);
            if (archival_duration.count() >= archival_interval_minutes) {
                std::cout << "Running Archival Job" << std::endl;
                archival_controller.applyArchivalPolicy();
                last_archival_run = now;
            }

            // Check if configured minutes have passed since last retention run
            auto retention_duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_retention_run);
            if (retention_duration.count() >= retention_interval_minutes) {
                std::cout << "Running Retention Job" << std::endl;
                retention_controller.applyRetentionPolicy();
                last_retention_run = now;
            }

            // Sleep for configured interval to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds));
        }
    }
};

int main() {
    if (prctl(PR_SET_NAME, "EFMS", 0, 0, 0) != 0) {
        perror("process control failed");
        return 1; 
    }
    
    try {
        JobScheduler scheduler;
        scheduler.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
