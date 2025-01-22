// main.cpp
#include <thread>
#include <chrono>
#include "ddsretentionpolicy.hpp"
#include "vecowretentionpolicy.hpp"
#include "archivalcontroller.hpp"
#include "retentioncontroller.hpp"
#include <iostream>  // Add this header

void archival_job() {
    vecowretentionpolicy vecow_retention_policy;
    auto policy = vecow_retention_policy.to_dict();
    ArchivalController archival_controller(
        policy,
        vecow_retention_policy.LOG_SOURCE,
        vecow_retention_policy.LOG_FILE_PATH
    );

    while (true) {
        archival_controller.applyArchivalPolicy();
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

void dds_retention_job() {
    ddsretentionpolicy dds_retention_policy;
    RetentionController retention_controller(
        dds_retention_policy.to_dict(),
        dds_retention_policy.LOG_FILE_PATH,
        dds_retention_policy.LOG_SOURCE
    );

    while (true) {
        retention_controller.applyRetentionPolicy();
        // std::this_thread::sleep_for(std::chrono::hours(2)); 
        std::this_thread::sleep_for(std::chrono::minutes(1));

    }
}

int main() {
    std::cout << "Initializing the DDS archival thread" << std::endl;
    std::thread archival_thread(archival_job);
    
    std::cout << "Initializing the VECOW retention thread" << std::endl;
    std::thread dds_thread(dds_retention_job);
    
    std::cout << "Starting the DDS archival thread" << std::endl;
    archival_thread.detach();
    
    std::cout << "Starting the VECOW retention thread" << std::endl;
    dds_thread.detach();

    // Keep main thread running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}