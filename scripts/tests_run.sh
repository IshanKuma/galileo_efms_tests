#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Navigate to the root directory (one level up from scripts folder)
cd "$(dirname "$0")/../tests"

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
sudo ./retentiontest

echo "Build and execution completed successfully."
