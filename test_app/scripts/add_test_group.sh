#!/bin/bash

# Script to add a new test group to the HeartBoxFirmware test_app
# Usage: ./add_test_group.sh <component_name>

set -e

if [ -z "$1" ]; then
    echo "Error: Component name is required"
    echo "Usage: $0 <component_name>"
    echo "Example: $0 gpio_task"
    exit 1
fi

COMPONENT_NAME="$1"
TEST_FILE="test_${COMPONENT_NAME}.c"
MAIN_DIR="$(cd "$(dirname "$0")/../main" && pwd)"
TEST_FILE_PATH="${MAIN_DIR}/${TEST_FILE}"
MAIN_C_PATH="${MAIN_DIR}/test_app_main.c"
CMAKE_PATH="${MAIN_DIR}/CMakeLists.txt"

echo "Adding test group for component: ${COMPONENT_NAME}"
echo "Test file path: ${TEST_FILE_PATH}"

# Check if test file already exists
if [ -f "${TEST_FILE_PATH}" ]; then
    echo "Error: Test file ${TEST_FILE} already exists"
    exit 1
fi

# Create the test file
cat > "${TEST_FILE_PATH}" << 'EOF'
#include "unity.h"
#include "unity_fixture.h"

// Include the component header
// #include "COMPONENT_NAME.h"

// Test group setup
TEST_GROUP(COMPONENT_NAME);
TEST_SETUP(COMPONENT_NAME)
{
  // Setup code here
}

TEST_TEAR_DOWN(COMPONENT_NAME)
{
  // Teardown code here
}

/** @brief Example test case
 *  @test Expected: Test passes
 */
TEST(COMPONENT_NAME, example_test)
{
  TEST_ASSERT_TRUE(1);
}

TEST_GROUP_RUNNER(COMPONENT_NAME)
{
  RUN_TEST_CASE(COMPONENT_NAME, example_test);
}
EOF

# Replace COMPONENT_NAME placeholder with actual component name
sed -i "s/COMPONENT_NAME/${COMPONENT_NAME}/g" "${TEST_FILE_PATH}"

echo "Created test file: ${TEST_FILE_PATH}"

# Add extern declaration to test_app_main.c
EXTERN_DECL="extern void TEST_${COMPONENT_NAME}_GROUP_RUNNER(void);"
if ! grep -q "${EXTERN_DECL}" "${MAIN_C_PATH}"; then
    # Find the line after the last extern declaration
    sed -i "/^extern void TEST_.*_GROUP_RUNNER(void);$/a ${EXTERN_DECL}" "${MAIN_C_PATH}"
    echo "Added extern declaration to ${MAIN_C_PATH}"
else
    echo "Extern declaration already exists in ${MAIN_C_PATH}"
fi

# Add RUN_TEST_GROUP to run_all_tests function (commented out by default)
RUN_TEST_LINE="  // RUN_TEST_GROUP(${COMPONENT_NAME});"
if ! grep -q "RUN_TEST_GROUP(${COMPONENT_NAME})" "${MAIN_C_PATH}"; then
    # Add before the closing brace of run_all_tests
    sed -i "/^static void run_all_tests(void)/,/^}$/ {
        /^}$/i\\
${RUN_TEST_LINE}
    }" "${MAIN_C_PATH}"
    echo "Added RUN_TEST_GROUP (commented) to ${MAIN_C_PATH}"
else
    echo "RUN_TEST_GROUP already exists in ${MAIN_C_PATH}"
fi

# Add source file to CMakeLists.txt
if ! grep -q "\"${TEST_FILE}\"" "${CMAKE_PATH}"; then
    # Find the SRCS line and add the new file
    sed -i "/idf_component_register(SRCS/,/)$/ {
        /test_app_main\.c/ s/$/\"\\n                       \"${TEST_FILE}\"/
    }" "${CMAKE_PATH}"
    
    # Clean up the formatting
    sed -i "s/\"\\\\n/\" \\\\\n/g" "${CMAKE_PATH}"
    
    echo "Added ${TEST_FILE} to ${CMAKE_PATH}"
else
    echo "${TEST_FILE} already exists in ${CMAKE_PATH}"
fi

echo ""
echo "Test group '${COMPONENT_NAME}' has been added successfully!"
echo ""
echo "Next steps:"
echo "1. Edit ${TEST_FILE_PATH} to implement your tests"
echo "2. Uncomment the RUN_TEST_GROUP line in test_app_main.c to enable the test group"
echo "3. If needed, add the component to REQUIRES in main/CMakeLists.txt"
echo "4. Build and run the test app: cd test_app && idf.py build flash monitor"
