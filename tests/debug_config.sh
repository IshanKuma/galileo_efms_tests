#!/bin/bash

# Debug script to diagnose config.json copy issues
echo "=== Config.json Copy Diagnostic ==="
echo

echo "1. Checking source config.json:"
CONFIG_SOURCE="../configuration/config.json"
if [ -f "$CONFIG_SOURCE" ]; then
    echo "✓ Source config.json exists at: $(realpath $CONFIG_SOURCE)"
    echo "  Size: $(stat -c%s $CONFIG_SOURCE) bytes"
    echo "  Permissions: $(stat -c%A $CONFIG_SOURCE)"
else
    echo "✗ Source config.json NOT FOUND at: $(realpath $CONFIG_SOURCE)"
fi
echo

echo "2. Checking build directory structure:"
echo "Current directory: $(pwd)"
echo "Build directory contents:"
ls -la
echo

echo "3. Checking for config.json in current directory:"
if [ -f "config.json" ]; then
    echo "✓ config.json exists in build/tests/"
    echo "  Size: $(stat -c%s config.json) bytes"
    echo "  Permissions: $(stat -c%A config.json)"
    echo "  First few lines:"
    head -5 config.json
else
    echo "✗ config.json NOT FOUND in build/tests/"
fi
echo

echo "4. Testing manual copy:"
if [ -f "$CONFIG_SOURCE" ]; then
    echo "Attempting manual copy..."
    cp "$CONFIG_SOURCE" ./config_manual.json
    if [ -f "./config_manual.json" ]; then
        echo "✓ Manual copy successful"
        rm ./config_manual.json
    else
        echo "✗ Manual copy failed"
    fi
else
    echo "✗ Cannot test manual copy - source not found"
fi
echo

echo "5. CMake variables (if available):"
echo "CMAKE_CURRENT_SOURCE_DIR would be: $(dirname $(pwd))/tests"
echo "TARGET_FILE_DIR would be: $(pwd)"
echo

echo "6. Filesystem permissions:"
echo "Current directory permissions: $(stat -c%A .)"
echo "Parent directory permissions: $(stat -c%A ..)"
echo

echo "=== End Diagnostic ==="
