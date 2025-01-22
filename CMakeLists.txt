# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Project name
project(EFMS_CPP_CODEBASE)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    src/archivalcontroller.cpp
    src/databaseservice.cpp
    src/fileservice.cpp
    src/main.cpp
    src/retentioncontroller.cpp
    src/vecowretentionpolicy.cpp    # Added missing source
    src/ddsretentionpolicy.cpp      # Added missing source
)

# Add header files
set(HEADERS
    include/archivalcontroller.hpp
    include/databaseservice.hpp
    include/fileservice.hpp
    include/retentioncontroller.hpp
    include/vecowretentionpolicy.hpp  # Added missing header
    include/ddsretentionpolicy.hpp    # Added missing header
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# Add include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PQXX_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${PQXX_LIBRARIES}
)

# Add library directories if needed
link_directories(${PQXX_LIBRARY_DIRS})

# Add compiler flags from pkg-config
target_compile_options(${PROJECT_NAME} PRIVATE ${PQXX_CFLAGS_OTHER})