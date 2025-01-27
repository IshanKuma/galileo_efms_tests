#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>

using json = nlohmann::json;
class TestLogger {
private:
    std::ofstream log_file;
    int total_tests = 0;
    int total_assertions = 0;
    int passed_assertions = 0;
    bool current_test_passed = true;
    int passed_tests = 0;

    std::string getCurrentDateTime() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
public:
    TestLogger(const std::string& filename = "Archival_policy_test_execution.log") {
        log_file.open(filename);
        log_file << "=================================================================" << std::endl;
        log_file << "FILE SERVICE TEST EXECUTION LOG" << std::endl;
        log_file << "=================================================================" << std::endl;
        log_file << "Execution Date: " << getCurrentDateTime() << std::endl;
    }
    void startTest(const std::string& testName) {
        if (total_tests > 0) {
            // Finalize previous test
            if (current_test_passed) {
                passed_tests++;
            }
        }

        log_file << "\n-----------------------------------------------------------------" << std::endl;
        log_file << "TEST CASE: " << testName << std::endl;
        total_tests++;
        current_test_passed = true;  // Reset for new test
    }
    void logCheck(const std::string& check, bool passed) {
        log_file << "- " << check << " - " << (passed ? "PASSED" : "FAILED") << std::endl;
        total_assertions++;
        if (passed) {
            passed_assertions++;
        } else {
            current_test_passed = false;
        }
    }
    void logCleanup(const std::string& message) {
        log_file << "Cleanup: " << message << std::endl;
    }
    void finalize() {
        // Handle the last test case
        if (current_test_passed) {
            passed_tests++;
        }

        double success_rate = (total_tests > 0) ? (passed_tests * 100.0 / total_tests) : 0;

        log_file << "\n=================================================================" << std::endl;
        log_file << "SUMMARY" << std::endl;
        log_file << "=================================================================" << std::endl;
        log_file << "Total Test Cases: " << total_tests << std::endl;
        log_file << "Passed Test Cases: " << passed_tests << std::endl;
        log_file << "Failed Test Cases: " << (total_tests - passed_tests) << std::endl;
        log_file << "Total Assertions: " << total_assertions << std::endl;
        log_file << "Passed Assertions: " << passed_assertions << std::endl;
        log_file << "Failed Assertions: " << (total_assertions - passed_assertions) << std::endl;
        log_file << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;
        log_file << "=================================================================" << std::endl;
        log_file.close();
    }
    ~TestLogger() {
        if (log_file.is_open()) {
            finalize();
        }
    }
};
// TestLogger logger;
// Global test logger
TestLogger logger;
// Forward declarations of the retention policy classes
class VECOWRetentionPolicy {
public:
    json to_dict() const {
        json policy;
        policy["PM"] = "PMX";
        policy["THRESHOLD_STORAGE_UTILIZATION"] = 75;
        policy["MOUNTED_PATH"] = "/mnt/storage";
        policy["SPATIAL_PATH"] = "/mnt/storage/Lam/Data/PMX/Spatial";
        policy["IS_RETENTION_POLICY_ENABLED"] = true;
        policy["RETENTION_PERIOD_IN_HOURS"] = 24 * 4;
        
        // Video retention policy
        policy["VIDEO_RETENTION_POLICY"] = {
            {"VIDEO_PATH", "/mnt/storage/Lam/Data/PMX/Spatial/Videos"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24 * 4},
            {"FILE_EXTENSIONS", std::vector<std::string>{"mp4", "mkv"}}
        };
        
        // Parquet retention policy
        policy["PARQUET_RETENTION_POLICY"] = {
            {"PARQUET_PATH", "/mnt/storage/Lam/Data/PMX/Spatial/Analysis"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24 * 90},
            {"FILE_EXTENSIONS", std::vector<std::string>{"parquet"}}
        };
        
        // Diagnostic retention policy
        policy["DIAGNOSTIC_RETENTION_POLICY"] = {
            {"DIAGNOSTIC_PATH", "/mnt/storage/Lam/Data/PMX/Spatial/Diagnostics"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24 * 30},
            {"FILE_EXTENSIONS", std::vector<std::string>{"csv", "json", "tx"}}
        };
        
        // Log retention policy
        policy["LOG_RETENTION_POLICY"] = {
            {"LOG_PATH", "/mnt/storage/Lam/Data/PMX/Spatial/Logs"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24 * 90},
            {"FILE_EXTENSIONS", std::vector<std::string>{"log"}}
        };
        
        // Video clips retention policy
        policy["VIDEO_CLIPS_RETENTION_POLICY"] = {
            {"VIDEO_CLIPS_PATH", "/mnt/storage/Lam/Data/PMX/Spatial/VideoClips"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24 * 90},
            {"FILE_EXTENSIONS", std::vector<std::string>{"mp4", "mkv"}}
        };
        
        policy["LOG_DIRECTORY"] = "/mnt/storage/Lam/Data/PMX/Spatial/Logs/General/ESK_EFMS";
        policy["LOG_SOURCE"] = "EFMS";
        policy["LOG_FILE"] = "VECOW_Retention_Archival.log";
        
        return policy;
    }
};

class DDSRetentionPolicy {
public:
    json to_dict() const {
        json policy;
        policy["PM"] = "PMX";
        policy["THRESHOLD_STORAGE_UTILIZATION"] = 85;
        policy["DDS_PATH"] = "/mnt/storage/dds/d";
        policy["SPATIAL_PATH"] = "/mnt/storage/dds/d/Lam/Data/PMX/Spatial";
        policy["IS_RETENTION_POLICY_ENABLED"] = true;
        policy["RETENTION_PERIOD_IN_HOURS"] = 24;
        
        // Video retention policy
        policy["VIDEO_RETENTION_POLICY"] = {
            {"VIDEO_PATH", "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Videos"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24},
            {"FILE_EXTENSIONS", std::vector<std::string>{"mp4", "mkv"}}
        };
        
        // Parquet retention policy
        policy["PARQUET_RETENTION_POLICY"] = {
            {"PARQUET_PATH", "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Analysis"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24},
            {"FILE_EXTENSIONS", std::vector<std::string>{"parquet"}}
        };
        
        // Diagnostic retention policy
        policy["DIAGNOSTIC_RETENTION_POLICY"] = {
            {"DIAGNOSTIC_PATH", "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Diagnostics"},
            {"IS_RETENTION_POLICY_ENABLED", true},
            {"RETENTION_PERIOD_IN_HOURS", 24},
            {"FILE_EXTENSIONS", std::vector<std::string>{"csv", "json", "tx"}}
        };
        
        // Log retention policy
        policy["LOG_RETENTION_POLICY"] = {
            {"LOG_PATH", "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Logs"}
        };
        
        return policy;
    }
};

class RetentionPolicyTest {
private:
    std::unique_ptr<VECOWRetentionPolicy> vecow_retention_policy;
    std::unique_ptr<DDSRetentionPolicy> dds_retention_policy;

public:
    void setUp() {
        vecow_retention_policy = std::make_unique<VECOWRetentionPolicy>();
        dds_retention_policy = std::make_unique<DDSRetentionPolicy>();
    }

    void tearDown() {
        vecow_retention_policy.reset();
        dds_retention_policy.reset();
    }

    const VECOWRetentionPolicy* getVecowPolicy() const { return vecow_retention_policy.get(); }
    const DDSRetentionPolicy* getDdsPolicy() const { return dds_retention_policy.get(); }
};

TEST_CASE("Test VECOW Retention Policy", "[vecow]") {
    RetentionPolicyTest test;
    test.setUp();
    logger.startTest("VECOW Retention Policy Test");

    auto vecow_retention = test.getVecowPolicy()->to_dict();
    
    SECTION("Basic Configuration") {
        bool check;
        
        check = vecow_retention["PM"] == "PMX";
        logger.logCheck("PM Configuration", check);
        REQUIRE(check);

        check = vecow_retention["THRESHOLD_STORAGE_UTILIZATION"] == 75;
        logger.logCheck("Storage Threshold", check);
        REQUIRE(check);

        check = vecow_retention["MOUNTED_PATH"] == "/mnt/storage";
        logger.logCheck("Mounted Path", check);
        REQUIRE(check);
    }

    SECTION("Video Retention Policy") {
        bool check;
        
        check = vecow_retention["VIDEO_RETENTION_POLICY"]["VIDEO_PATH"] == "/mnt/storage/Lam/Data/PMX/Spatial/Videos";
        logger.logCheck("Video Path", check);
        REQUIRE(check);

        check = vecow_retention["VIDEO_RETENTION_POLICY"]["IS_RETENTION_POLICY_ENABLED"] == true;
        logger.logCheck("Video Retention Enabled", check);
        REQUIRE(check);

        check = vecow_retention["VIDEO_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"] == 24 * 4;
        logger.logCheck("Video Retention Period", check);
        REQUIRE(check);
    }

    SECTION("Parquet Retention Policy") {
        bool check;
        
        check = vecow_retention["PARQUET_RETENTION_POLICY"]["PARQUET_PATH"] == "/mnt/storage/Lam/Data/PMX/Spatial/Analysis";
        logger.logCheck("Parquet Path", check);
        REQUIRE(check);

        check = vecow_retention["PARQUET_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"] == 24 * 90;
        logger.logCheck("Parquet Retention Period", check);
        REQUIRE(check);
    }

    logger.logCleanup("Cleaning up VECOW test resources");
    test.tearDown();
}

TEST_CASE("Test DDS Retention Policy", "[dds]") {
    RetentionPolicyTest test;
    test.setUp();
    logger.startTest("DDS Retention Policy Test");

    auto dds_retention = test.getDdsPolicy()->to_dict();
    
    SECTION("Basic Configuration") {
        bool check;
        
        check = dds_retention["PM"] == "PMX";
        logger.logCheck("PM Configuration", check);
        REQUIRE(check);

        check = dds_retention["THRESHOLD_STORAGE_UTILIZATION"] == 85;
        logger.logCheck("Storage Threshold", check);
        REQUIRE(check);

        check = dds_retention["DDS_PATH"] == "/mnt/storage/dds/d";
        logger.logCheck("DDS Path", check);
        REQUIRE(check);
    }

    SECTION("Video Retention Policy") {
        bool check;
        
        check = dds_retention["VIDEO_RETENTION_POLICY"]["VIDEO_PATH"] == "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Videos";
        logger.logCheck("Video Path", check);
        REQUIRE(check);

        check = dds_retention["VIDEO_RETENTION_POLICY"]["RETENTION_PERIOD_IN_HOURS"] == 24;
        logger.logCheck("Video Retention Period", check);
        REQUIRE(check);
    }

    SECTION("Diagnostic Retention Policy") {
        bool check;
        
        check = dds_retention["DIAGNOSTIC_RETENTION_POLICY"]["DIAGNOSTIC_PATH"] == "/mnt/storage/dds/d/Lam/Data/PMX/Spatial/Diagnostics";
        logger.logCheck("Diagnostic Path", check);
        REQUIRE(check);

        check = dds_retention["DIAGNOSTIC_RETENTION_POLICY"]["FILE_EXTENSIONS"] == std::vector<std::string>{"csv", "json", "tx"};
        logger.logCheck("Diagnostic File Extensions", check);
        REQUIRE(check);
    }

    logger.logCleanup("Cleaning up DDS test resources");
    test.tearDown();
}