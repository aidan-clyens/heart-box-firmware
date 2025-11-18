@echo off
REM Script to build and flash ESP32 device with specified device name
REM Usage: flash_device.bat <device_name>
REM Example: flash_device.bat Heart_Box_1

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Error: Device name required
    echo Usage: flash_device.bat ^<device_name^>
    echo Example: flash_device.bat Heart_Box_1
    exit /b 1
)

set DEVICE_NAME=%~1
set COM_PORT=COM4

echo ========================================
echo Building and flashing device: %DEVICE_NAME%
echo COM Port: %COM_PORT%
echo ========================================
echo.

REM Set the device name in sdkconfig
echo Setting CONFIG_HEARTBOX_DEVICE_NAME="%DEVICE_NAME%"
powershell -Command "(Get-Content sdkconfig) -replace '^CONFIG_HEARTBOX_DEVICE_NAME=.*', 'CONFIG_HEARTBOX_DEVICE_NAME=\"%DEVICE_NAME%\"' | Set-Content sdkconfig"

if errorlevel 1 (
    echo Failed to update configuration
    exit /b 1
)

echo.
echo Running idf.py reconfigure...
idf.py reconfigure

if errorlevel 1 (
    echo Reconfigure failed
    exit /b 1
)

echo.
echo Running idf.py build...
idf.py build

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo Running idf.py flash on %COM_PORT%...
idf.py -p %COM_PORT% flash

if errorlevel 1 (
    echo Flash failed
    exit /b 1
)

echo.
echo ========================================
echo Successfully flashed %DEVICE_NAME% to %COM_PORT%
echo ========================================

endlocal
