cmake_minimum_required(VERSION 3.10)
project(retention_tests)
 
# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
# Create test executable
add_executable(test_retention
    test_retentionpolicy.cpp
)
 
# Add include paths - both parent directory and include folder
target_include_directories(test_retention PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
 
# Add source files needed by the test (excluding main.cpp)
target_sources(test_retention PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ddsretentionpolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/vecowretentionpolicy.cpp
)