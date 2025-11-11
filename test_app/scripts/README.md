# Test App Scripts

This directory contains scripts to automate common tasks for the HeartBoxFirmware test application.

## add_test_group

Automatically creates a new test group for testing a component.

### Usage

**Bash (Linux/Mac/Git Bash):**
```bash
cd test_app/scripts
./add_test_group.sh <component_name>
```

**Windows (Command Prompt or PowerShell):**
```batch
cd test_app\scripts
add_test_group.bat <component_name>
```

### Example

```bash
# Add a new test group for the wifi_task component
./add_test_group.sh wifi_task
```

### What it does

The script performs the following actions:

1. **Creates a new test file** (`test_<component_name>.c`) in the `test_app/main/` directory with:
   - Unity test framework boilerplate
   - TEST_GROUP, TEST_SETUP, and TEST_TEAR_DOWN macros
   - An example test case
   - TEST_GROUP_RUNNER implementation

2. **Updates `test_app_main.c`**:
   - Adds the extern declaration for the test group runner
   - Adds a commented-out `RUN_TEST_GROUP()` call in the `run_all_tests()` function

3. **Updates `main/CMakeLists.txt`**:
   - Adds the new test file to the `SRCS` list in `idf_component_register()`

### After Running the Script

1. **Implement your tests**: Edit the generated `test_<component_name>.c` file to add your actual test cases
2. **Enable the test group**: Uncomment the `RUN_TEST_GROUP()` line in `test_app_main.c`
3. **Add component dependencies** (if needed): Add the component to the `REQUIRES` list in `main/CMakeLists.txt`
4. **Build and run**: 
   ```bash
   cd test_app
   idf.py build flash monitor
   ```

### File Structure

After adding a test group for `wifi_task`, your structure will look like:

```
test_app/
├── main/
│   ├── CMakeLists.txt              (updated)
│   ├── test_app_main.c             (updated)
│   ├── test_file_system.c
│   ├── test_gpio_task.c
│   ├── test_state_machine_task.c
│   └── test_wifi_task.c            (new)
└── scripts/
    ├── add_test_group.sh
    ├── add_test_group.bat
    └── README.md
```

### Example Test File Template

The script generates a test file with this structure:

```c
#include "unity.h"
#include "unity_fixture.h"

// Include the component header
// #include "component_name.h"

// Test group setup
TEST_GROUP(component_name);
TEST_SETUP(component_name)
{
  // Setup code here
}

TEST_TEAR_DOWN(component_name)
{
  // Teardown code here
}

/** @brief Example test case
 *  @test Expected: Test passes
 */
TEST(component_name, example_test)
{
  TEST_ASSERT_TRUE(1);
}

TEST_GROUP_RUNNER(component_name)
{
  RUN_TEST_CASE(component_name, example_test);
}
```

### Troubleshooting

- **"Test file already exists" error**: The test group has already been created. Check `test_app/main/` for existing test files.
- **Build errors after adding component**: Make sure to add the component name to the `REQUIRES` list in `test_app/main/CMakeLists.txt`.
- **Tests not running**: Ensure you've uncommented the `RUN_TEST_GROUP()` line in `test_app_main.c`.
