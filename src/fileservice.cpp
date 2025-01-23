#include "fileservice.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <stdexcept>
#include <regex>
#include <cstdlib>

namespace fs = std::filesystem;

/**
 * Default constructor for FileService class
 */
FileService::FileService() {}

/**
 * Default destructor for FileService class
 * Virtual to allow for proper cleanup in derived classes
 */
// FileService::~FileService() {}

/**
 * Determines the type of a given path
 * param path The filesystem path to check
 * return "file" if path is a regular file
 *         "directory" if path is a directory
 *         "not exist" if path doesn't exist
 */
std::string FileService::check_path_type(const std::string& path) {
    if (fs::is_regular_file(path)) {
        return "file";
    } else if (fs::is_directory(path)) {
        return "directory";
    }
    return "not exist";
}

/**
 * Creates a new directory at the specified path
 * param path The path where the directory should be created
 * throws std::runtime_error if directory already exists
 */
void FileService::create_directory(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            // Create directories
            fs::create_directories(path);

            // Set owner permissions (read, write, execute)
            fs::permissions(
                path, 
                fs::perms::owner_all,  // Owner: Read, Write, Execute
                fs::perm_options::add  // Add these permissions
            );

            std::cout << "Directory created successfully: " << path << std::endl;
        } else {
            // Directory exists, do nothing
            std::cout << "Directory already exists: " << path << std::endl;
        }
    } catch (const fs::filesystem_error& e) {
        // Log and rethrow filesystem-specific errors
        std::cerr << "Filesystem error: " << e.what() << " [" << e.path1() << "]" << std::endl;
        throw;
    } catch (const std::exception& e) {
        // Log and rethrow generic exceptions
        std::cerr << "Error: " << e.what() << std::endl;
        throw;
    }
}




/**
 * Lists all files and directories in the specified path (non-recursive)
 * param path The directory path to read
 * return Vector of strings containing paths of all entries
 */
std::vector<std::string> FileService::read_directory(const std::string& path) {
    std::vector<std::string> files_and_directories;

    for (const auto& entry : fs::directory_iterator(path)) {
        files_and_directories.push_back(entry.path().string());
    }
    return files_and_directories;
}

/**
 * Recursively reads a directory, separating files and directories
 * Sorts both files and directories by modification time
 * param path The directory path to read recursively
 * return Pair of vectors containing file paths and directory paths
 */
std::pair<std::vector<std::string>, std::vector<std::string>> FileService::read_directory_recursively(const std::string& path) {
    std::vector<std::string> files;
    std::vector<std::string> directories;

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (fs::is_regular_file(entry.path())) {
            files.push_back(entry.path().string());
        } else if (fs::is_directory(entry.path())) {
            directories.push_back(entry.path().string());
        }
    }

    // Sort files and directories by modification time
    std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
        struct stat stat_a, stat_b;
        stat(a.c_str(), &stat_a);
        stat(b.c_str(), &stat_b);
        return stat_a.st_mtime < stat_b.st_mtime;
    });
    
    std::sort(directories.begin(), directories.end(), [](const std::string& a, const std::string& b) {
        struct stat stat_a, stat_b;
        stat(a.c_str(), &stat_a);
        stat(b.c_str(), &stat_b);
        return stat_a.st_mtime < stat_b.st_mtime;
    });

    return {files, directories};
}

/**
 * Deletes an empty directory
 * param path The path of the directory to delete
 * throws std::runtime_error if directory is not empty
 */
void FileService::delete_directory(const std::string& path) {
    if (fs::is_empty(path)) {
        fs::remove(path);
    } else {
        throw std::runtime_error("Directory is not empty.");
    }
}

/**
 * Checks if a directory is empty
 * param directory The path to check
 * return true if directory is empty, false otherwise
 */
bool FileService::is_directory_empty(const std::string& directory) {
    return fs::is_empty(directory);
}

/**
 * Checks directory permissions for specific operations
 * Currently only implements DELETE permission check
 * param path The directory path to check
 * param permission The permission type to verify
 * return true if permission is granted, false otherwise
 */
bool FileService::check_directory_permissions(const std::string& path, Permission permission) {
    try {
        // Ensure path exists
        if (!fs::exists(path)) {
            std::cerr << "Path does not exist: " << path << std::endl;
            return false;
        }

        // Get current file permissions
        fs::perms current_perms = fs::status(path).permissions(); // Add semicolon here

        switch (permission) {
            case Permission::DELETE: 
                return (current_perms & fs::perms::owner_write) != fs::perms::none;
            case Permission::WRITE:
                return (current_perms & fs::perms::owner_write) != fs::perms::none;
            case Permission::READ:
                return (current_perms & fs::perms::owner_read) != fs::perms::none;
            case Permission::EXECUTE:
                return (current_perms & fs::perms::owner_exec) != fs::perms::none;
            default:
                return false;
        }
    } 
    catch (const std::exception& e) {
        std::cerr << "Permission check error: " << e.what() << std::endl;
        return false;
    }
}

/**
 * Creates a new file at the specified path
 * param filename The path where the file should be created
 * throws std::runtime_error if file already exists or creation fails
 */
void FileService::create_file(const std::string& filename) {
    if (!fs::exists(filename)) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to create file.");
        }
        file.close();
        if (!fs::exists(filename)) {
            throw std::runtime_error("File creation verification failed.");
        }
    } else {
        throw std::runtime_error("File already exists.");
    }
}

/**
 * Checks if a file exists
 * param file_path The path to check
 * return true if file exists, false otherwise
 */
bool FileService::file_exists(const std::string& file_path) {
    return fs::exists(file_path);
}

/**
 * Deletes a file at the specified path
 * param filename The path of the file to delete
 * throws std::runtime_error if file doesn't exist
 */
void FileService::delete_file(const std::string& filename) {
    if (fs::exists(filename)) {
        fs::remove(filename);
    } else {
        throw std::runtime_error("File not found.");
    }
}

/**
 * Checks file permissions
 * param filename The file to check
 * param permission The permission type to verify
 * return true if file exists and is regular file
 */
bool FileService::check_file_permissions(const std::string& filename, Permission permission) {
    return fs::is_regular_file(filename);
}

/**
 * Copies files with bandwidth limiting using rsync
 * param source Source path
 * param destination Destination path
 * param bandwidth_limit Bandwidth limit in KB/s
 * Note: This is Unix/Linux specific implementation
 */
void FileService::copy_files(const std::string& source, const std::string& destination, int bandwidth_limit) {
    try {
        // Create parent directory of destination
        std::string dest_parent_dir = fs::path(destination).parent_path().string();
        std::cout << "dest_parent_dir" << dest_parent_dir << std::endl;
        // if (check_directory_permissions(dest_parent_dir, Permission::WRITE)) {
        create_directory(dest_parent_dir);
        // }

        // Construct rsync command 
        std::string command = "rsync -av --bwlimit=" + std::to_string(bandwidth_limit) + 
                              " \"" + source + "\" \"" + destination + "\"";

        std::cout << "Copying files: " << command << std::endl;

        // Execute command and check result
        int result = std::system(command.c_str());
        if (result != 0) {
            throw std::runtime_error("File copy failed with exit code: " + std::to_string(result));
        }

        std::cout << "Files copied successfully." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Copy error: " << e.what() << std::endl;
        throw;
    }
}

/**
 * Gets memory details for a filesystem path
 * param path The path to check
 * return Tuple of {total_memory, used_memory, free_memory} in filesystem units
 * Note: This is Unix/Linux specific implementation
 */
std::tuple<uint64_t, uint64_t, uint64_t> FileService::get_memory_details(const std::string& path) {
    try {
        // Validate path
        if (path.empty()) {
            throw std::runtime_error("Empty path provided to get_memory_details");
        }

        // Use popen to execute df command
        // Add -P flag for POSIX output format
        std::string command = "df -P \"" + path + "\" | awk 'NR==2{print $2, $3, $4}'";
        FILE* fp = popen(command.c_str(), "r");
        if (!fp) {
            throw std::runtime_error("Failed to execute df command");
        }

        // Read values
        uint64_t total_kb = 0, used_kb = 0, free_kb = 0;
        char buffer[128];
        
        // Get the output
        if (fgets(buffer, sizeof(buffer), fp) == nullptr) {
            pclose(fp);
            throw std::runtime_error("Failed to read df command output");
        }

        // Parse the output
        if (sscanf(buffer, "%lu %lu %lu", &total_kb, &used_kb, &free_kb) != 3) {
            pclose(fp);
            throw std::runtime_error("Failed to parse disk space values");
        }

        // Close the pipe
        int status = pclose(fp);
        if (status == -1) {
            throw std::runtime_error("Failed to close command pipe");
        }

        // Validate values
        if (total_kb == 0) {
            throw std::runtime_error("Invalid total memory value (zero)");
        }

        // Convert KB to bytes
        const uint64_t KB_TO_BYTES = 1024;
        uint64_t total_bytes = total_kb * KB_TO_BYTES;
        uint64_t used_bytes = used_kb * KB_TO_BYTES;
        uint64_t free_bytes = free_kb * KB_TO_BYTES;

        std::cout << "Disk space details for " << path << ":" << std::endl;
        std::cout << "Total: " << total_kb << " KB (" << total_bytes << " bytes)" << std::endl;
        std::cout << "Used: " << used_kb << " KB (" << used_bytes << " bytes)" << std::endl;
        std::cout << "Free: " << free_kb << " KB (" << free_bytes << " bytes)" << std::endl;

        return {total_bytes, used_bytes, free_bytes};

    } catch (const std::exception& e) {
        std::cerr << "Error in get_memory_details: " << e.what() << std::endl;
        throw; // Re-throw to let caller handle the error
    }
}


/**
 * Gets total used memory for a filesystem path
 * param path The path to check
 * return Amount of used memory
 */
float FileService::get_total_memory_used(const std::string& path) {
    auto [total_memory, used_memory, free_memory] = get_memory_details(path);
    return used_memory;
}

/**
 * Gets total available memory for a filesystem path
 * param path The path to check
 * return Amount of free memory
 */
float FileService::get_total_available_memory(const std::string& path) {
    auto [total_memory, used_memory, free_memory] = get_memory_details(path);
    return free_memory;
}

/**
 * Calculates file age in hours
 * param filename The file to check
 * return Age of file in hours as float
 */
float FileService::get_file_age_in_hours(const std::string& filename) {
    struct stat file_stat;
    stat(filename.c_str(), &file_stat);
    float file_age_seconds = time(NULL) - file_stat.st_mtime;
    // std::cout << "file_age_seconds" << file_age_seconds << std::endl;
    return file_age_seconds / 3600;
}

/**
 * Checks if a mounted drive is accessible
 * param mounted_path The path to check
 * return true if path exists and is accessible
 */
bool FileService::is_mounted_drive_accessible(const std::string& mounted_path) {
    return fs::exists(mounted_path);
}