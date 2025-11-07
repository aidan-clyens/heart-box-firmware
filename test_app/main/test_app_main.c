#include "unity.h"
#include "unity_fixture.h"

#include "file_system.h"
#include "state_machine_task.h"

// Test group setup
TEST_GROUP(state_machine_task);
TEST_SETUP(state_machine_task)
{
  file_system_init();
  state_machine_task_init();
}

TEST_TEAR_DOWN(state_machine_task)
{
}

// Testcases
TEST(state_machine_task, example)
{
  TEST_ASSERT_EQUAL(1, 1);
}

TEST_GROUP_RUNNER(state_machine_task)
{
  RUN_TEST_CASE(state_machine_task, example);
}

static void run_all_tests(void)
{
  RUN_TEST_GROUP(state_machine_task);
}

// Test application main
void app_main(void)
{
  UNITY_MAIN_FUNC(run_all_tests);
}