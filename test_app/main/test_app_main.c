#include "unity.h"
#include "unity_fixture.h"

extern void TEST_state_machine_task_GROUP_RUNNER(void);

static void run_all_tests(void)
{
  RUN_TEST_GROUP(state_machine_task);
}

// Test application main
void app_main(void)
{
  UNITY_MAIN_FUNC(run_all_tests);
}