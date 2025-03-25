# # Compiler and flags
# CXX = g++
# CXXFLAGS = -std=c++17 -Wall -O2

# # Include and lib flags
# PKG_CONFIG = `pkg-config --cflags --libs libpqxx libmodbus libzmq`
# LDFLAGS = -lutilities_lib -pthread

# # Source files and target
# SRC = src/archivalcontroller.cpp \
#       src/main.cpp \
#       src/retentioncontroller.cpp \
#       src/vecowretentionpolicy.cpp \
#       src/ddsretentionpolicy.cpp

# TARGET = EFMS_CPP_CODEBASE

# # Default target
# all: $(TARGET)

# # Linking
# $(TARGET): $(SRC)
# 	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(PKG_CONFIG) $(LDFLAGS)

# # Clean
# clean:
# 	rm -f $(TARGET)

# # Run target
# run:
# 	./$(TARGET)

# .PHONY: all clean run

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Iinclude  # Added -Iinclude for header files

# Include and lib flags
PKG_CONFIG = `pkg-config --cflags --libs libpqxx libmodbus libzmq`
LDFLAGS = -lutilities_lib -pthread

# Source files and target
SRC = src/archivalcontroller.cpp \
      src/main.cpp \
      src/retentioncontroller.cpp \
      src/vecowretentionpolicy.cpp \
      src/ddsretentionpolicy.cpp

TARGET = EFMS_CPP_CODEBASE

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(PKG_CONFIG) $(LDFLAGS)

# Clean
clean:
	rm -f $(TARGET)

# Run target
# run:
# 	./$(TARGET)

run:
	cd	configuration && ../$(TARGET)

.PHONY: all clean run

