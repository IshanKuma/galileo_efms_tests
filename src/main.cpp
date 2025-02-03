// main.cpp
//-----------------2 thread implementation---------------------------
// #include <thread>
// #include <chrono>
// #include "ddsretentionpolicy.hpp"
// #include "vecowretentionpolicy.hpp"
// #include "archivalcontroller.hpp"
// #include "retentioncontroller.hpp"
// #include <iostream>  // Add this header

// void archival_job() {
//     vecowretentionpolicy vecow_retention_policy;
//     auto policy = vecow_retention_policy.to_dict();
//     ArchivalController archival_controller(
//         policy,
//         vecow_retention_policy.LOG_SOURCE,
//         vecow_retention_policy.LOG_FILE_PATH
//     );

//     while (true) {
//         archival_controller.applyArchivalPolicy();
//         std::this_thread::sleep_for(std::chrono::minutes(5));
//     }
// }

// void dds_retention_job() {
//     ddsretentionpolicy dds_retention_policy;
//     RetentionController retention_controller(
//         dds_retention_policy.to_dict(),
//         dds_retention_policy.LOG_FILE_PATH,
//         dds_retention_policy.LOG_SOURCE
//     );

//     while (true) {
//         retention_controller.applyRetentionPolicy();
//         // std::this_thread::sleep_for(std::chrono::hours(2)); 
//         std::this_thread::sleep_for(std::chrono::minutes(5));

//     }
// }

// int main() {
//     std::cout << "Initializing the DDS archival thread" << std::endl;
//     std::thread archival_thread(archival_job);
    
//     std::cout << "Initializing the VECOW retention thread" << std::endl;
//     std::thread dds_thread(dds_retention_job);
    
//     std::cout << "Starting the DDS archival thread" << std::endl;
//     archival_thread.detach();
    
//     std::cout << "Starting the VECOW retention thread" << std::endl;
//     dds_thread.detach();

//     // Keep main thread running
//     while (true) {
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }

//     return 0;
// }

//-----------------single thread implementation---------------------------
#include <chrono>
#include <iostream>
#include "ddsretentionpolicy.hpp"
#include "vecowretentionpolicy.hpp"
#include "archivalcontroller.hpp"
#include "retentioncontroller.hpp"
#include <thread>

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
    {}

    void run() {
        while (true) {
            auto now = std::chrono::steady_clock::now();
            
            // Check if 5 minutes have passed since last archival run
            auto archival_duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_archival_run);
            if (archival_duration.count() >= 5) {                       // change 5 to required time 
                std::cout << "Running Archival Job" << std::endl;
                archival_controller.applyArchivalPolicy();
                last_archival_run = now;
            }

            // Check if 5 minutes have passed since last retention run
            auto retention_duration = std::chrono::duration_cast<std::chrono::minutes>(now - last_retention_run);
            if (retention_duration.count() >= 5) {                    // change 5 to required time 
                std::cout << "Running Retention Job" << std::endl;
                retention_controller.applyRetentionPolicy();
                last_retention_run = now;
            }

            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};

int main() {
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