# # Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name
project(EFMS)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add path to find installed utilities library
set(CMAKE_PREFIX_PATH "/usr/local/lib")

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
find_package(nlohmann_json REQUIRED)

# Find additional required packages that utilities_lib depends on
pkg_check_modules(MODBUS REQUIRED libmodbus)
pkg_check_modules(ZMQ REQUIRED libzmq)

# Find utilities library
find_library(UTILITIES_LIB utilities_lib REQUIRED)

# Add source files
set(SOURCES
    src/archivalcontroller.cpp
    src/main.cpp
    src/retentioncontroller.cpp
    src/vecowretentionpolicy.cpp
    src/ddsretentionpolicy.cpp
)

# Create executable using only source files
add_executable(${PROJECT_NAME} ${SOURCES})

# Add include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PQXX_INCLUDE_DIRS}
        ${MODBUS_INCLUDE_DIRS}
        ${ZMQ_INCLUDE_DIRS}
        /usr/local/include  # Add this to find utilities headers
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        ${PQXX_LIBRARIES}
        ${MODBUS_LIBRARIES}
        ${ZMQ_LIBRARIES}
        nlohmann_json::nlohmann_json
        ${UTILITIES_LIB}
)

# Add library directories
link_directories(
    ${PQXX_LIBRARY_DIRS}
    ${MODBUS_LIBRARY_DIRS}
    ${ZMQ_LIBRARY_DIRS}
)

# Add compiler flags from pkg-config
target_compile_options(${PROJECT_NAME} PRIVATE 
    ${PQXX_CFLAGS_OTHER}
    ${MODBUS_CFLAGS_OTHER}
    ${ZMQ_CFLAGS_OTHER}
)

# ======================================================================
# Enable and Add Test Suite
# ======================================================================
enable_testing()

add_subdirectory(tests)

# Config file copying is handled in tests/CMakeLists.txt