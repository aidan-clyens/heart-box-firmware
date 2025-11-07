#include "unity.h"
#include "unity_fixture.h"

// Test group setup
TEST_GROUP(testable);
TEST_SETUP(testable)
{
}

TEST_TEAR_DOWN(testable)
{
}

// Testcases
TEST(testable, example)
{
  TEST_ASSERT_EQUAL(1, 1);
}

TEST_GROUP_RUNNER(testable)
{
  RUN_TEST_CASE(testable, example);
}

static void run_all_tests(void)
{
  RUN_TEST_GROUP(testable);
}

// Test application main
void app_main(void)
{
  UNITY_MAIN_FUNC(run_all_tests);
}