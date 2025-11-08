#include "unity.h"
#include "unity_fixture.h"

#include "file_system.h"
#include "gpio_task.h"
#include "wifi_task.h"
#include "state_machine_task.h"

// Test group setup
TEST_GROUP(state_machine_task);
TEST_SETUP(state_machine_task)
{
  file_system_init();
  gpio_task_init();
  wifi_task_init();

  vTaskDelay(pdMS_TO_TICKS(2000)); // Allow some time for initialization
}

TEST_TEAR_DOWN(state_machine_task)
{
}

/** @brief Test: State Machine initializes to STATE_PROVISIONING state when no WiFi credentials are stored
 */
TEST(state_machine_task, initialize_no_wifi_credentials)
{
  file_system_clear(NVS_SSID_KEY);
  file_system_clear(NVS_PASSWORD_KEY);

  state_machine_task_init();

  vTaskDelay(pdMS_TO_TICKS(2000)); // Allow some time for initialization

  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);
}

TEST_GROUP_RUNNER(state_machine_task)
{
  RUN_TEST_CASE(state_machine_task, initialize_no_wifi_credentials);
}
