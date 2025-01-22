#ifndef FILESERVICE_HPP
#define FILESERVICE_HPP

#include <string>
#include <vector>
#include <ctime>

enum class Permission {
    EXISTS = 0x01,
    READ = 0x02,
    WRITE = 0x04,
    EXECUTE = 0x08,
    DELETE = 0x0C  // Custom permission to represent delete
};

class FileService {
public:
    FileService();

    // Methods for file operations
    static std::string check_path_type(const std::string& path);
    void create_directory(const std::string& path);
    std::vector<std::string> read_directory(const std::string& path);
    std::pair<std::vector<std::string>, std::vector<std::string>> read_directory_recursively(const std::string& path);
    void delete_directory(const std::string& path);
    bool is_directory_empty(const std::string& directory);
    bool check_directory_permissions(const std::string& path, Permission permission);
    void create_file(const std::string& filename);
    bool file_exists(const std::string& file_path);
    void delete_file(const std::string& filename);
    bool check_file_permissions(const std::string& filename, Permission permission);
    void copy_files(const std::string& source, const std::string& destination, int bandwidth_limit = 10240);
    // std::tuple<float, float, float> get_memory_details(const std::string& path);
    std::tuple<uint64_t, uint64_t, uint64_t> get_memory_details(const std::string& path);

    float get_total_memory_used(const std::string& path);
    float get_total_available_memory(const std::string& path);
    float get_file_age_in_hours(const std::string& filename);
    bool is_mounted_drive_accessible(const std::string& mounted_path);
};

#endif // FILESERVICE_HPP
