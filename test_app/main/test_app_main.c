#include "unity.h"
#include "unity_fixture.h"

extern void TEST_state_machine_task_GROUP_RUNNER(void);
extern void TEST_file_system_GROUP_RUNNER(void);

static void run_all_tests(void)
{
  RUN_TEST_GROUP(state_machine_task);
  // RUN_TEST_GROUP(file_system);
}

// Test application main
void app_main(void)
{
  UNITY_MAIN_FUNC(run_all_tests);
}