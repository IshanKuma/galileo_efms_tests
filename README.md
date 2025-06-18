# GALILEO EFMS

## Overview
GALILEO EFMS is a C++ based Edge File Management System that handles file management with a focus on retention and archival capabilities. The system uses real services for archival control, retention management, and policy enforcement without mock dependencies.

## Features
* Real-time archival policy implementation
* Intelligent retention policy management
* Comprehensive logging service integration
* DDS (Data Distribution Service) retention policy configuration
* Vecow hardware-specific retention policy configuration
* Full integration test suite with real services
* CMake-based build system

## Installation Requirements

### System Requirements
* Ubuntu 18.04+ or compatible Linux distribution
* C++17 compatible compiler (GCC 7.0+ or Clang 5.0+)
* CMake 3.10 or higher
* Make build system
* Git for cloning the repository

### Core Dependencies

#### Required System Packages
```bash
# Update package lists
sudo apt-get update

# Install build essentials
sudo apt-get install build-essential cmake git

# Install pkg-config for dependency management
sudo apt-get install pkg-config

# Install nlohmann-json library
sudo apt-get install nlohmann-json3-dev

# Install additional utilities
sudo apt-get install rsync curl
```

### Dependencies

#### CPP-Utilities Package
GALILEO EFMS depends on the CPP-Utilities package for core functionality. Install it before proceeding:
```bash
# If you have the .deb package
sudo dpkg -i cpp-utilities_<version>_<architecture>.deb
sudo apt-get install -f  # Install any missing dependencies

# Verify installation
dpkg -l | grep cpp-utilities
```

#### Database Requirements
* PostgreSQL version 16.6 or above
* Libpqxx version 6.4.5 or above

#### Communication Libraries  
* libmodbus for device communication
* libzmq for messaging

### Installing Dependencies

#### PostgreSQL
```bash
# For Ubuntu/Debian
sudo apt-get update
sudo apt-get install postgresql-16 postgresql-client-16
sudo apt-get install postgresql-contrib-16

# Start PostgreSQL service
sudo systemctl start postgresql
sudo systemctl enable postgresql
```

#### Libpqxx
```bash
# For Ubuntu/Debian
sudo apt-get install libpqxx-dev
```

#### Communication Libraries
```bash
# Install libmodbus
sudo apt-get install libmodbus-dev

# Install libzmq
sudo apt-get install libzmq3-dev
```

## Project Setup and Installation

### 1. Clone the Repository
```bash
git clone <repository-url>
cd galileo-efms
```

### 2. Database Setup
```bash
# Create database user and database
sudo -u postgres createuser --interactive --pwprompt galileo_user
sudo -u postgres createdb -O galileo_user esk_galileo

# Test database connection
psql -h localhost -U galileo_user -d esk_galileo -c "SELECT version();"
```

### 3. Configuration
The system uses `configuration/config.json` for all settings. Ensure this file contains:
- Database connection parameters
- Retention policy configurations  
- Archival policy settings
- Scheduler intervals

### 4. Build the Project

#### Clean Build (Recommended for first-time setup)
```bash
# Remove any existing build directory
rm -rf build

# Create fresh build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the entire project
make
```

#### Build Just the Tests
```bash
# From the build directory
make efms_tests
```

### 5. Verify Installation
```bash
# Check if config.json was copied correctly
ls -la tests/config.json

# Verify config content
head -10 tests/config.json
```

## Running the Application

### Main Application
```bash
# From project root directory
cd scripts
./main_run.sh
```

### Running Tests

#### Complete Test Suite
```bash
# From build/tests directory
cd build/tests
./efms_tests --reporter=console
```

#### Run Specific Test Cases
```bash
# Configuration tests only
./efms_tests "1. Configuration & Policy Initialization Tests"

# Job scheduling tests only  
./efms_tests "5. Job Scheduling & Integration Tests"

# All tests with verbose output
./efms_tests --reporter=console --verbosity=high
```

#### Test Results
- **Expected Result**: All 5 test cases should pass
- **Total Assertions**: ~30+ assertions across all test cases
- **Test Coverage**: Real services integration, no mocks

### Using Legacy Scripts
```bash
# Alternative method using provided scripts
cd scripts
./tests_run.sh
```

## Troubleshooting

### Common Issues and Solutions

#### 1. Config File Missing Error
**Error**: `Failed to open config.json`
**Solution**:
```bash
# Rebuild the project to regenerate config.json
cd build
make clean
make efms_tests

# Verify config file exists
ls -la tests/config.json
```

#### 2. Test Failures After Code Changes
**Error**: Some tests fail after modifying source code
**Solution**:
```bash
# Always rebuild when tests fail
rm -rf build
mkdir build && cd build
cmake ..
make efms_tests
cd tests
./efms_tests
```

#### 3. Database Connection Issues
**Error**: Database connection failures in tests
**Solution**:
```bash
# Check PostgreSQL service
sudo systemctl status postgresql

# Verify database exists
psql -h localhost -U postgres -c "\l" | grep esk_galileo

# Check configuration/config.json database settings
```

#### 4. Missing Dependencies
**Error**: Library not found during compilation
**Solution**:
```bash
# Reinstall all dependencies
sudo apt-get update
sudo apt-get install libpqxx-dev libmodbus-dev libzmq3-dev nlohmann-json3-dev

# Verify pkg-config can find libraries
pkg-config --libs libpqxx libmodbus libzmq
```

#### 5. CMake Configuration Issues  
**Error**: CMake fails to configure
**Solution**:
```bash
# Clear CMake cache and reconfigure
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
```

### Build System Notes

#### When to Rebuild
- **Always rebuild** when tests fail unexpectedly
- **Always rebuild** after pulling new changes from repository
- **Always rebuild** after modifying CMakeLists.txt files
- **Clean rebuild recommended** when switching between different development environments

#### Build Directory Management
- The `build/` directory is excluded from git (see .gitignore)
- Always safe to delete and recreate the build directory
- Config files are automatically copied during build process

#### Debug vs Release Builds
```bash
# Debug build (default)
cmake ..

# Release build (optimized)
cmake .. -DCMAKE_BUILD_TYPE=Release
```

## Development

### Project Architecture
The project follows a modular architecture with real service integration:

#### Core Components
* **ArchivalController**: Manages file archival operations with real file system integration
* **RetentionController**: Handles file retention policies with actual file lifecycle management  
* **VecowRetentionPolicy**: Hardware-specific retention policies for Vecow systems
* **DdsRetentionPolicy**: Data Distribution Service retention management
* **JobScheduler**: Coordinates scheduled operations using real configuration

#### Real Services Integration
The system uses actual implementations (no mocks):
* **FileService**: Real file system operations, disk space monitoring
* **DatabaseService**: Live PostgreSQL database connections
* **LoggingService**: Actual log file creation and management
* **PolicyEngines**: Real-time policy evaluation and enforcement

### Integration with CPP-Utilities
GALILEO EFMS utilizes the following services from CPP-Utilities:
* File Management Service for disk operations
* Database Service for PostgreSQL connectivity
* Logging Service for structured logging
* Memory Management utilities

When developing new features or modifying existing ones, ensure compatibility with the CPP-Utilities interfaces. The header files from CPP-Utilities are automatically available after package installation.

### Testing Strategy
* **Real Service Testing**: All tests use actual service implementations
* **Integration Testing**: End-to-end testing with real file operations
* **Configuration Testing**: Real JSON configuration loading and validation
* **Database Testing**: Live database connections during test execution
* **No Mock Dependencies**: Eliminated all mock objects for authentic testing

### Code Quality Guidelines
* Use C++17 standards and features
* Follow RAII principles for resource management
* Implement proper exception handling
* Maintain comprehensive logging for debugging
* Ensure thread-safety for concurrent operations

### Adding New Tests
When adding new test cases:
1. Use real service implementations
2. Follow the existing TestSetup pattern
3. Clean up resources properly in destructors
4. Test with actual configuration files
5. Verify database connections and file operations

## Project Structure
```
galileo-efms/
├── CMakeLists.txt           # Main build configuration
├── configuration/
│   └── config.json          # System configuration
├── include/                 # Header files
├── src/                     # Source implementations
│   ├── archivalcontroller.cpp
│   ├── retentioncontroller.cpp
│   ├── vecowretentionpolicy.cpp
│   ├── ddsretentionpolicy.cpp
│   └── main.cpp
├── tests/                   # Real service integration tests
│   ├── CMakeLists.txt       # Test build configuration
│   ├── main_test.cpp        # Comprehensive test suite
│   └── debug_config.sh      # Configuration diagnostics
├── scripts/                 # Utility scripts
└── third_party/            # External dependencies
    └── catch2/             # Testing framework
```

## Support and Maintenance

### Getting Help
For support and queries:
1. Check the troubleshooting section above
2. Verify all dependencies are properly installed
3. Ensure database is running and accessible
4. Try a clean rebuild before reporting issues
5. Create an issue in the project repository with:
   - Full error messages
   - System information (OS, compiler version)
   - Steps to reproduce the problem
   - Build logs if compilation fails

### Version Information
- **Build System**: CMake 3.10+
- **C++ Standard**: C++17
- **Testing Framework**: Catch2 v2.13.10
- **Database**: PostgreSQL 16.6+
- **Dependencies**: See installation requirements above

### Performance Notes
- Tests may take 30-60 seconds due to real file operations
- Database connections are established during test execution
- Large file operations use rsync with bandwidth limiting
- Log files are created in real-time during testing

---

## Quick Start Summary

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake git pkg-config
sudo apt-get install libpqxx-dev libmodbus-dev libzmq3-dev nlohmann-json3-dev

# 2. Clone and build
git clone <repository-url>
cd galileo-efms
rm -rf build && mkdir build && cd build
cmake ..
make efms_tests

# 3. Run tests
cd tests
./efms_tests --reporter=console

# 4. If tests fail, rebuild
cd ../..
rm -rf build && mkdir build && cd build
cmake .. && make efms_tests
cd tests && ./efms_tests
```

---
*Note: This project uses real service integration for authentic testing. Build directory is excluded from git and should be regenerated locally.*
