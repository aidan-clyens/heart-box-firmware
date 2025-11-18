#!/bin/bash
# Script to build and flash ESP32 device with specified device name
# Usage: ./flash_device.sh <device_name>
# Example: ./flash_device.sh Heart_Box_1

set -e  # Exit on error

if [ -z "$1" ]; then
    echo "Error: Device name required"
    echo "Usage: ./flash_device.sh <device_name>"
    echo "Example: ./flash_device.sh Heart_Box_1"
    exit 1
fi

DEVICE_NAME="$1"
COM_PORT="COM4"

echo "========================================"
echo "Building and flashing device: $DEVICE_NAME"
echo "COM Port: $COM_PORT"
echo "========================================"
echo

# Set the device name in sdkconfig
echo "Setting CONFIG_HEARTBOX_DEVICE_NAME=\"$DEVICE_NAME\""
sed -i "s/^CONFIG_HEARTBOX_DEVICE_NAME=.*/CONFIG_HEARTBOX_DEVICE_NAME=\"$DEVICE_NAME\"/" sdkconfig

if [ $? -ne 0 ]; then
    echo "Failed to update configuration"
    exit 1
fi

echo
echo "Running idf.py reconfigure..."
idf.py reconfigure

if [ $? -ne 0 ]; then
    echo "Reconfigure failed"
    exit 1
fi

echo
echo "Running idf.py build..."
idf.py build

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

echo
echo "Running idf.py flash on $COM_PORT..."
idf.py -p $COM_PORT flash

if [ $? -ne 0 ]; then
    echo "Flash failed"
    exit 1
fi

echo
echo "========================================"
echo "Successfully flashed $DEVICE_NAME to $COM_PORT"
echo "========================================"
