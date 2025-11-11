@echo off
REM Script to add a new test group to the HeartBoxFirmware test_app
REM Usage: add_test_group.bat <component_name>

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Error: Component name is required
    echo Usage: %~nx0 ^<component_name^>
    echo Example: %~nx0 gpio_task
    exit /b 1
)

set "COMPONENT_NAME=%~1"
set "TEST_FILE=test_%COMPONENT_NAME%.c"
set "SCRIPT_DIR=%~dp0"
set "MAIN_DIR=%SCRIPT_DIR%..\main"
set "TEST_FILE_PATH=%MAIN_DIR%\%TEST_FILE%"
set "MAIN_C_PATH=%MAIN_DIR%\test_app_main.c"
set "CMAKE_PATH=%MAIN_DIR%\CMakeLists.txt"

echo Adding test group for component: %COMPONENT_NAME%
echo Test file path: %TEST_FILE_PATH%
echo.

REM Check if test file already exists
if exist "%TEST_FILE_PATH%" (
    echo Error: Test file %TEST_FILE% already exists
    exit /b 1
)

REM Create the test file
(
echo #include "unity.h"
echo #include "unity_fixture.h"
echo.
echo // Include the component header
echo // #include "%COMPONENT_NAME%.h"
echo.
echo // Test group setup
echo TEST_GROUP^(%COMPONENT_NAME%^);
echo TEST_SETUP^(%COMPONENT_NAME%^)
echo {
echo   // Setup code here
echo }
echo.
echo TEST_TEAR_DOWN^(%COMPONENT_NAME%^)
echo {
echo   // Teardown code here
echo }
echo.
echo /** @brief Example test case
echo  *  @test Expected: Test passes
echo  */
echo TEST^(%COMPONENT_NAME%, example_test^)
echo {
echo   TEST_ASSERT_TRUE^(1^);
echo }
echo.
echo TEST_GROUP_RUNNER^(%COMPONENT_NAME%^)
echo {
echo   RUN_TEST_CASE^(%COMPONENT_NAME%, example_test^);
echo }
) > "%TEST_FILE_PATH%"

echo Created test file: %TEST_FILE_PATH%

REM Add extern declaration to test_app_main.c
set "EXTERN_DECL=extern void TEST_%COMPONENT_NAME%_GROUP_RUNNER(void);"
findstr /C:"%EXTERN_DECL%" "%MAIN_C_PATH%" >nul
if errorlevel 1 (
    REM Create a temporary file with the updated content
    set "TEMP_FILE=%TEMP%\test_app_main_temp.c"
    set "FOUND_EXTERN=0"
    
    for /f "usebackq delims=" %%a in ("%MAIN_C_PATH%") do (
        set "LINE=%%a"
        echo !LINE!>> "!TEMP_FILE!"
        
        REM Check if this is an extern declaration line
        echo !LINE! | findstr /C:"extern void TEST_.*_GROUP_RUNNER(void);" >nul
        if not errorlevel 1 (
            set "FOUND_EXTERN=1"
        ) else (
            REM If we just finished the extern declarations section, add the new one
            if "!FOUND_EXTERN!"=="1" (
                if not "!LINE:extern=!"=="!LINE!" (
                    REM Still in extern section, skip
                ) else (
                    REM First non-extern line after extern section
                    REM Go back and insert before this line
                    set "FOUND_EXTERN=2"
                )
            )
        )
    )
    
    REM If we found extern declarations, insert the new one
    if "!FOUND_EXTERN!" gtr "0" (
        set "INSERTED=0"
        type nul > "!TEMP_FILE!"
        
        for /f "usebackq delims=" %%a in ("%MAIN_C_PATH%") do (
            set "LINE=%%a"
            
            REM Check if this line is an extern declaration
            echo !LINE! | findstr /C:"extern void TEST_.*_GROUP_RUNNER(void);" >nul
            if not errorlevel 1 (
                echo !LINE!>> "!TEMP_FILE!"
                set "LAST_EXTERN=1"
            ) else (
                REM If we just passed the last extern and haven't inserted yet
                if "!LAST_EXTERN!"=="1" if "!INSERTED!"=="0" (
                    echo %EXTERN_DECL%>> "!TEMP_FILE!"
                    set "INSERTED=1"
                    set "LAST_EXTERN=0"
                )
                echo !LINE!>> "!TEMP_FILE!"
            )
        )
        
        move /y "!TEMP_FILE!" "%MAIN_C_PATH%" >nul
        echo Added extern declaration to test_app_main.c
    ) else (
        echo Warning: Could not find extern declarations section in test_app_main.c
        echo Please manually add: %EXTERN_DECL%
    )
) else (
    echo Extern declaration already exists in test_app_main.c
)

REM Add RUN_TEST_GROUP to run_all_tests function (commented out by default)
set "RUN_TEST_LINE=  // RUN_TEST_GROUP(%COMPONENT_NAME%);"
findstr /C:"RUN_TEST_GROUP(%COMPONENT_NAME%)" "%MAIN_C_PATH%" >nul
if errorlevel 1 (
    set "TEMP_FILE=%TEMP%\test_app_main_temp2.c"
    set "IN_FUNCTION=0"
    type nul > "!TEMP_FILE!"
    
    for /f "usebackq delims=" %%a in ("%MAIN_C_PATH%") do (
        set "LINE=%%a"
        
        REM Check if we're entering the run_all_tests function
        echo !LINE! | findstr /C:"static void run_all_tests(void)" >nul
        if not errorlevel 1 (
            set "IN_FUNCTION=1"
        )
        
        REM If we're in the function and hit the closing brace, insert before it
        if "!IN_FUNCTION!"=="1" (
            echo !LINE! | findstr /R "^}$" >nul
            if not errorlevel 1 (
                echo %RUN_TEST_LINE%>> "!TEMP_FILE!"
                set "IN_FUNCTION=0"
            )
        )
        
        echo !LINE!>> "!TEMP_FILE!"
    )
    
    move /y "!TEMP_FILE!" "%MAIN_C_PATH%" >nul
    echo Added RUN_TEST_GROUP ^(commented^) to test_app_main.c
) else (
    echo RUN_TEST_GROUP already exists in test_app_main.c
)

REM Add source file to CMakeLists.txt
findstr /C:"\"%TEST_FILE%\"" "%CMAKE_PATH%" >nul
if errorlevel 1 (
    set "TEMP_FILE=%TEMP%\CMakeLists_temp.txt"
    type nul > "!TEMP_FILE!"
    
    for /f "usebackq delims=" %%a in ("%CMAKE_PATH%") do (
        set "LINE=%%a"
        echo !LINE!>> "!TEMP_FILE!"
        
        REM If this line contains test_app_main.c, add our file on the next line
        echo !LINE! | findstr /C:"test_app_main.c" >nul
        if not errorlevel 1 (
            echo                        "%TEST_FILE%">> "!TEMP_FILE!"
        )
    )
    
    move /y "!TEMP_FILE!" "%CMAKE_PATH%" >nul
    echo Added %TEST_FILE% to CMakeLists.txt
) else (
    echo %TEST_FILE% already exists in CMakeLists.txt
)

echo.
echo Test group '%COMPONENT_NAME%' has been added successfully!
echo.
echo Next steps:
echo 1. Edit %TEST_FILE_PATH% to implement your tests
echo 2. Uncomment the RUN_TEST_GROUP line in test_app_main.c to enable the test group
echo 3. If needed, add the component to REQUIRES in main\CMakeLists.txt
echo 4. Build and run the test app: cd test_app ^&^& idf.py build flash monitor

endlocal
