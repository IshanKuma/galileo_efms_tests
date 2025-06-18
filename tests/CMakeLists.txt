# Set minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name for tests
project(EFMSTests)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages (same as main project)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
find_package(nlohmann_json REQUIRED)

# Find utilities library
find_library(UTILITIES_LIB utilities_lib REQUIRED)

# Create the test executable
add_executable(efms_tests
    main_test.cpp
)

# Add source files from the main application that we need to test
# Exclude main.cpp to avoid duplicate main() symbols
target_sources(efms_tests PRIVATE
    ../src/archivalcontroller.cpp
    ../src/retentioncontroller.cpp
    ../src/ddsretentionpolicy.cpp
    ../src/vecowretentionpolicy.cpp
    # Note: main.cpp is NOT included here
)

# Add include directories
target_include_directories(efms_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/catch2/include # For catch.hpp
    ${PQXX_INCLUDE_DIRS}
    /usr/local/include # For utilities headers
)

# Link libraries to the test executable
target_link_libraries(efms_tests PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${PQXX_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${UTILITIES_LIB}
)

# Add command to run tests with 'make test' or 'ctest'
add_test(NAME efms_all_tests COMMAND efms_tests)

# Ensure config file is copied to the build directory for tests to find
add_custom_command(
    TARGET efms_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/../configuration/config.json
            $<TARGET_FILE_DIR:efms_tests>/config.json
    COMMENT "Copying config.json to test directory"
)