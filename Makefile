# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./include

# Build directory
BUILD_DIR = build

# Source files
SOURCES = src/archivalcontroller.cpp \
          src/databaseutilities.cpp \
          src/fileservice.cpp \
          src/main.cpp \
          src/retentioncontroller.cpp

# Object files
OBJECTS = $(SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)

# Executable name
EXECUTABLE = $(BUILD_DIR)/EFMS_CPP_CODEBASE

# Main target
all: $(BUILD_DIR) $(EXECUTABLE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link the executable
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@

# Compile source files
$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Clean and rebuild
# rebuild: clean all

# Test target (if using Catch2)
# test: all
# 	$(CXX) $(CXXFLAGS) tests/*.cpp -o $(BUILD_DIR)/test_runner
# 	./$(BUILD_DIR)/test_runner

# .PHONY: all clean rebuild test