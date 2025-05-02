#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Kill any existing instances
echo "Killing any existing EFMS_CPP_CODEBASE processes..."  
sudo pkill EFMS_CPP_CODEBASE 2>/dev/null || true
sudo pkill -f "sudo.*EFMS_CPP_CODEBASE" 2>/dev/null || true

# Navigate to the root directory (one level up from scripts folder)
cd "$(dirname "$0")/.."

# Check if the build directory exists
if [ -d "build" ]; then
    echo "Build directory exists. Deleting it..."
    rm -rf build
fi

# Create the build directory
echo "Creating build directory..."
mkdir build

# Navigate to the build directory
cd build

# Run cmake and make
echo "Running cmake..."
cmake ..

echo "Running make..."
make

# Run the executable
echo "Running executable..."
sudo ./EFMS

echo "Build and execution completed successfully."