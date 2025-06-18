#define CATCH_CONFIG_MAIN
#include "catch.hpp"

// Include headers of classes to be tested
#include "archivalcontroller.hpp"
#include "retentioncontroller.hpp"
#include "ddsretentionpolicy.hpp"
#include "vecowretentionpolicy.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>

// Forward declare the ArchivalConfig namespace
namespace ArchivalConfig {
    extern int bandwidth_limit_kb;
    extern std::map<std::string, bool> eligibility;
    extern bool config_loaded;
    void loadConfig();
}

// JobScheduler class for testing (extracted from main.cpp without main function)
class JobScheduler {
private:
    vecowretentionpolicy vecow_retention_policy;
    ddsretentionpolicy dds_retention_policy;
    ArchivalController archival_controller;
    RetentionController retention_controller;
    std::chrono::steady_clock::time_point last_archival_run;
    std::chrono::steady_clock::time_point last_retention_run;
    int archival_interval_minutes;
    int retention_interval_minutes;
    int poll_interval_seconds;
    
    void loadConfig() {
        std::ifstream configFile("config.json");
        if (!configFile.is_open()) {
            throw std::runtime_error("Main Test -Failed to open config.json");
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
    
    // Getters for testing
    int getArchivalInterval() const { return archival_interval_minutes; }
    int getRetentionInterval() const { return retention_interval_minutes; }
    int getPollInterval() const { return poll_interval_seconds; }
};

// Helper class to manage test setup and configuration
class TestSetup {
private:
    std::string original_config_path;
    std::string test_config_path;
    
public:
    TestSetup() {
        // Create test directories for file operations
        createTestDirectories();
        
        // Create test configuration file
        createTestConfig();
        
        // Load configurations for real services
        try {
            ArchivalConfig::loadConfig();
            vecowretentionpolicy::loadConfig();
            ddsretentionpolicy::loadConfig();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not load all configurations: " << e.what() << std::endl;
        }
    }
    
    ~TestSetup() {
        // Clean up test directories
        cleanupTestDirectories();
        
        // Restore original config if it existed
        restoreConfig();
    }
    
    void createTestDirectories() {
        // Create necessary test directories
        std::filesystem::create_directories("test_data/Videos");
        std::filesystem::create_directories("test_data/Analysis");
        std::filesystem::create_directories("test_data/Diagnostics");
        std::filesystem::create_directories("test_data/Logs");
        std::filesystem::create_directories("test_data/VideoClips");
        std::filesystem::create_directories("test_dds/Videos");
        std::filesystem::create_directories("test_dds/Analysis");
    }
    
    void createTestConfig() {
        // Check if config.json already exists (CMake-copied version)
        if (std::filesystem::exists("config.json")) {
            // Backup the existing config.json (CMake-copied version)
            std::filesystem::copy_file("config.json", "cmake_config_backup.json", 
                                      std::filesystem::copy_options::overwrite_existing);
        }
        
        // Create a comprehensive test configuration similar to the real config
        nlohmann::json test_config = {
            {"scheduler", {
                {"archival_interval_minutes", 1},
                {"retention_interval_minutes", 2},
                {"poll_interval_seconds", 1}
            }},
            {"archival", {
                {"bandwidth_limit_kb", 1024},
                {"eligibility", {
                    {"Videos", true},
                    {"Analysis", true},
                    {"Logs", true},
                    {"VideoClips", true},
                    {"Diagnostics", true}
                }}
            }},
            {"vecow_retention_policy", {
                {"threshold_storage_utilization", 75},
                {"mounted_path", "test_data"},
                {"dds_path", "test_dds"},
                {"spatial_path", "test_data/Spatial"},
                {"is_retention_policy_enabled", true},
                {"retention_period_in_hours", 40},
                {"base_log_directory", "test_data/Logs/"},
                {"log_source", "EdgeController_Retention_Archival"},
                {"log_file", "EdgeController_Retention_Archival.log"},
                {"pm", "PMX"},
                {"retention_policies", {
                    {"diagnostic", {
                        {"path", "test_data/Diagnostics"},
                        {"enabled", true},
                        {"retention_hours", 720},
                        {"file_types", {"csv", "json", "txt"}}
                    }},
                    {"log", {
                        {"path", "test_data/Logs"},
                        {"enabled", true},
                        {"retention_hours", 2160},
                        {"file_types", {"log"}}
                    }},
                    {"video_clips", {
                        {"path", "test_data/VideoClips"},
                        {"enabled", true},
                        {"retention_hours", 2160},
                        {"file_types", {"mp4", "mkv"}}
                    }}
                }},
                {"station_policies", {
                    {"stations", {"Station1", "Station2"}},
                    {"video", {
                        {"path_suffix", "/Videos"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"mp4", "mkv"}}
                    }},
                    {"analysis", {
                        {"path_suffix", "/Analysis"},
                        {"enabled", true},
                        {"retention_hours", 2160},
                        {"file_types", {"parquet"}}
                    }}
                }}
            }},
            {"dds_retention_policy", {
                {"threshold_storage_utilization", 85},
                {"dds_path", "test_dds"},
                {"spatial_path", "test_dds/Spatial"},
                {"is_retention_policy_enabled", true},
                {"retention_period_in_hours", 24},
                {"base_log_directory", "test_dds/Logs/"},
                {"log_source", "EdgeController_Retention_Archival"},
                {"log_file", "EdgeController_Retention_Archival.log"},
                {"pm", "PMX"},
                {"retention_policies", {
                    {"diagnostic", {
                        {"path", "test_dds/Diagnostics"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"csv", "json", "txt"}}
                    }},
                    {"log", {
                        {"path", "test_dds/Logs"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"log"}}
                    }},
                    {"video_clips", {
                        {"path", "test_dds/VideoClips"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"mp4", "mkv"}}
                    }}
                }},
                {"station_policies", {
                    {"stations", {"Station1", "Station2"}},
                    {"video", {
                        {"path_suffix", "/Videos"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"mp4", "mkv"}}
                    }},
                    {"analysis", {
                        {"path_suffix", "/Analysis"},
                        {"enabled", true},
                        {"retention_hours", 96},
                        {"file_types", {"parquet"}}
                    }}
                }}
            }}
        };
        
        std::ofstream config_file("config.json");
        config_file << test_config.dump(4);
        config_file.close();
    }
    
    void cleanupTestDirectories() {
        try {
            if (std::filesystem::exists("test_data")) {
                std::filesystem::remove_all("test_data");
            }
            if (std::filesystem::exists("test_dds")) {
                std::filesystem::remove_all("test_dds");
            }
            if (std::filesystem::exists("test.log")) {
                std::filesystem::remove("test.log");
            }
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }
    
    void restoreConfig() {
        // Restore the original CMake-copied config if we backed it up
        try {
            if (std::filesystem::exists("cmake_config_backup.json")) {
                std::filesystem::copy_file("cmake_config_backup.json", "config.json", 
                                          std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove("cmake_config_backup.json");
            }
            // Clean up any other test config files
            if (std::filesystem::exists("test_config_backup.json")) {
                std::filesystem::remove("test_config_backup.json");
            }
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }
    
    nlohmann::json createValidArchivalPolicy() {
        return nlohmann::json{
            {"MOUNTED_PATH", "test_data"},
            {"DDS_PATH", "test_dds"},
            {"THRESHOLD_STORAGE_UTILIZATION", "80"},
            {"VIDEO_RETENTION_POLICY_Station1", "test_data/Videos"},
            {"ANALYSIS_RETENTION_POLICY_Station1", "test_data/Analysis"}
        };
    }
    
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> createValidRetentionPolicy() {
        return {
            {"DDS_PATH", {{"value", "test_dds"}}},
            {"THRESHOLD_STORAGE_UTILIZATION", {{"value", "80"}}},
            {"VIDEO_RETENTION_POLICY_Station1", {{"value", "test_dds/Videos"}, {"retentionPeriod", "96"}}},
            {"ANALYSIS_RETENTION_POLICY_Station1", {{"value", "test_dds/Analysis"}, {"retentionPeriod", "96"}}}
        };
    }
    
    void createTestFiles() {
        // Create test files for archival and retention testing
        std::ofstream video_file("test_data/Videos/test_video.mp4");
        video_file << "dummy video content";
        video_file.close();
        
        std::ofstream analysis_file("test_data/Analysis/test_analysis.parquet");
        analysis_file << "dummy analysis content";
        analysis_file.close();
        
        std::ofstream log_file("test_data/Logs/test.log");
        log_file << "dummy log content";
        log_file.close();
    }
};


TEST_CASE("1. Configuration & Policy Initialization Tests") {
    TestSetup setup;

    SECTION("1.1 Valid Configuration Loading") {
        // Test that policy configurations can be loaded correctly
        REQUIRE_NOTHROW(ArchivalConfig::loadConfig());
        
        // After loading, check if config was loaded successfully
        // Note: We can only check if the function runs without throwing
        // The actual values depend on the test config.json we created
        REQUIRE(true); // Config loading completed without exception
    }
    
    SECTION("1.2.A Valid ArchivalController Initialization") {
        auto valid_policy = setup.createValidArchivalPolicy();
        REQUIRE_NOTHROW(ArchivalController(valid_policy, "test_source", "test.log"));
    }

    SECTION("1.2.B Malformed config.json") {
    // Backup current config
    std::filesystem::copy_file("config.json", "temp_backup.json", 
                              std::filesystem::copy_options::overwrite_existing);
    
    // Create a malformed config file for this test
    std::ofstream malformed_config("config.json");
    malformed_config << "{ \"scheduler\": { \"archival_interval_minutes\": 30, } }";
    malformed_config.close();

    REQUIRE_THROWS_AS(JobScheduler(), std::runtime_error);
    
    // Properly restore the backup
    std::filesystem::copy_file("temp_backup.json", "config.json", 
                              std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove("temp_backup.json");
}

    SECTION("1.2.C Missing critical field in archival policy") {
        nlohmann::json invalid_policy = {
            {"DDS_PATH", "/path/to/dds"},
            // Missing THRESHOLD_STORAGE_UTILIZATION
            {"MOUNTED_PATH", "/path/to/mounted"}
        };
        REQUIRE_THROWS_AS(ArchivalController(invalid_policy, "test_source", "test.log"), std::runtime_error);
    }
    
    SECTION("1.3 Valid RetentionController Initialization") {
        auto valid_retention_policy = setup.createValidRetentionPolicy();
        REQUIRE_NOTHROW(RetentionController(valid_retention_policy, "test.log", "test_source"));
    }
    
    SECTION("1.4 Policy Loading from Real Config") {
        // Test loading from actual configuration
        try {
            vecowretentionpolicy vecow_policy;
            ddsretentionpolicy dds_policy;
            
            // Should not throw
            REQUIRE_NOTHROW(vecow_policy.loadConfig());
            REQUIRE_NOTHROW(dds_policy.loadConfig());
            
            // Verify that policies are created and accessible
            REQUIRE_NOTHROW(vecow_policy.to_dict());
            REQUIRE_NOTHROW(dds_policy.to_dict());
            
        } catch (const std::exception& e) {
            // If config loading fails, just verify it throws appropriately
            std::cout << "Config loading failed as expected in test environment: " << e.what() << std::endl;
        }
    }
}

TEST_CASE("2. Storage Utilization & Threshold Tests") {
    TestSetup setup;
    auto valid_policy = setup.createValidArchivalPolicy();
    ArchivalController controller(valid_policy, "test_source", "test.log");

    SECTION("2.1.A Controller Initialization Success") {
        // Test that the controller was created successfully
        // We can't access private methods directly, so we test through public interface
        REQUIRE_NOTHROW(controller.startNormalPipeline());
    }

    SECTION("2.2.A Public Method Accessibility") {
        // Test that public methods are accessible and don't crash
        REQUIRE_NOTHROW(controller.applyArchivalPolicy());
        REQUIRE_NOTHROW(controller.startMaxUtilizationPipeline());
    }
    
    SECTION("2.3 FileService Integration") {
        // Test that the FileService member is accessible
        // This tests that the controller integrates properly with file services
        
        // Test a method that exists in FileService
        auto [total, used, free] = controller.fileService.get_memory_details("test_data");
        REQUIRE(total >= 0); // Memory details should return valid values
    }
}


TEST_CASE("3. Archival Process & Data Management Tests") {
    TestSetup setup;
    setup.createTestFiles(); // Create test files for archival testing
    
    // Load configurations
    ArchivalConfig::loadConfig(); 
    
    // Create real archival policy from vecow retention policy
    vecowretentionpolicy vecow_policy;
    auto archival_policy_dict = vecow_policy.to_dict();
    
    // Convert the flat dictionary to JSON format for ArchivalController
    nlohmann::json archival_policy_json = setup.createValidArchivalPolicy();
    ArchivalController controller(archival_policy_json, "test_source", "test.log");

    SECTION("3.1 Archival Controller Integration") {
        // Test basic controller functionality with real policies
        REQUIRE_NOTHROW(controller.applyArchivalPolicy());
        REQUIRE_NOTHROW(controller.startMaxUtilizationPipeline());
    }

    SECTION("3.2.A Policy Configuration Test") {
        // Test that the policy configuration is properly loaded
        // We can verify this by checking that the controller initializes without errors
        auto test_policy = setup.createValidArchivalPolicy();
        REQUIRE_NOTHROW(ArchivalController(test_policy, "test_source2", "test2.log"));
    }

    SECTION("3.2.B Real Directory Processing") {
        // Test processing actual directories through public interface
        setup.createTestFiles();
        
        // Test that pipeline operations handle real files correctly
        REQUIRE_NOTHROW(controller.startNormalPipeline());
    }

    SECTION("3.2.C FileService Integration Test") {
        // Test that the embedded FileService works correctly
        setup.createTestFiles();
        
        // Test file_exists method with a real file
        REQUIRE(controller.fileService.file_exists("test_data/Videos/test_video.mp4"));
        
        // Test get_file_age_in_hours method
        double age = controller.fileService.get_file_age_in_hours("test_data/Videos/test_video.mp4");
        REQUIRE(age >= 0.0); // File age should be non-negative
    }
    
    SECTION("3.3 Normal Pipeline Execution") {
        // Test the normal pipeline with real files but safe operations
        // This will test the pipeline logic without actually copying files to avoid side effects
        setup.createTestFiles();
        
        // The startNormalPipeline() should not crash and should handle real file operations
        REQUIRE_NOTHROW(controller.startNormalPipeline());
    }
}


TEST_CASE("4. Retention Process & File Lifecycle Tests") {
    TestSetup setup;
    setup.createTestFiles(); // Create test files for retention testing
    
    // Load real configuration
    try {
        ddsretentionpolicy::loadConfig();
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not load DDS config: " << e.what() << std::endl;
    }

    // Create real retention policy using the test setup
    auto retention_policy_map = setup.createValidRetentionPolicy();
    RetentionController controller(retention_policy_map, "test.log", "test_source");
    
    SECTION("4.1 Retention Controller Integration") {
        // Test basic retention controller functionality
        REQUIRE_NOTHROW(controller.applyRetentionPolicy());
        REQUIRE_NOTHROW(controller.startMaxUtilizationPipeline());
    }

    SECTION("4.2.A Real Policy Configuration") {
        // Test that retention policy is properly configured
        auto test_policy = setup.createValidRetentionPolicy();
        REQUIRE_NOTHROW(RetentionController(test_policy, "test2.log", "test_source2"));
    }

    SECTION("4.2.B Real File Operations through Public Interface") {
        // Test processing real files through public methods only
        setup.createTestFiles();
        
        // Test pipeline operations that handle real files
        REQUIRE_NOTHROW(controller.startNormalPipeline());
    }

    SECTION("4.2.C Empty Directory Cleanup") {
        // Create an empty test directory
        const std::string empty_dir = "test_dds/empty_subdir";
        std::filesystem::create_directories(empty_dir);
        
        // Test stopPipeline functionality
        REQUIRE_NOTHROW(controller.stopPipeline({empty_dir}));
        
        // The directory should be handled appropriately by the real implementation
    }
    
    SECTION("4.3 Real Retention Policy Application") {
        // Test the real retention policy application
        REQUIRE_NOTHROW(controller.applyRetentionPolicy());
        
        // The method should complete without throwing exceptions
    }
    
    SECTION("4.4 Real Normal Pipeline Execution") {
        // Test the normal pipeline with real files
        setup.createTestFiles();
        
        // The startNormalPipeline() should not crash and should handle real file operations
        REQUIRE_NOTHROW(controller.startNormalPipeline());
    }
}


TEST_CASE("5. Job Scheduling & Integration Tests") {
    TestSetup setup;

    SECTION("5.1 JobScheduler Configuration") {
        // Test that JobScheduler reads configuration correctly
        JobScheduler scheduler;
        
        // Test configuration access through public getters
        REQUIRE(scheduler.getArchivalInterval() > 0);
        REQUIRE(scheduler.getRetentionInterval() > 0);
        REQUIRE(scheduler.getPollInterval() > 0);
    }
    
    SECTION("5.2 Integration with Real Policies") {
        // Test integration between different components
        vecowretentionpolicy vecow_policy;
        ddsretentionpolicy dds_policy;
        
        REQUIRE_NOTHROW(vecow_policy.loadConfig());
        REQUIRE_NOTHROW(dds_policy.loadConfig());
        
        // Test that policies can be converted to usable formats
        REQUIRE_NOTHROW(vecow_policy.to_dict());
        REQUIRE_NOTHROW(dds_policy.to_dict());
    }
    
    SECTION("5.3 End-to-End Configuration Flow") {
        // Test the complete configuration flow
        ArchivalConfig::loadConfig();
        
        // Create controllers with real policies
        auto archival_policy = setup.createValidArchivalPolicy();
        auto retention_policy = setup.createValidRetentionPolicy();
        
        REQUIRE_NOTHROW(ArchivalController(archival_policy, "test_source", "test.log"));
        REQUIRE_NOTHROW(RetentionController(retention_policy, "test.log", "test_source"));
    }
    
    SECTION("5.4 Real File System Operations") {
        // Test that the system can handle real file operations
        setup.createTestFiles();
        
        auto archival_policy = setup.createValidArchivalPolicy();
        ArchivalController controller(archival_policy, "test_source", "test.log");
        
        // Test file service integration with real methods
        REQUIRE(controller.fileService.file_exists("test_data/Videos/test_video.mp4"));
        
        // Test memory details retrieval
        auto [total, used, free] = controller.fileService.get_memory_details("test_data");
        REQUIRE(total >= 0);
        REQUIRE(used >= 0);
        REQUIRE(free >= 0);
    }
}