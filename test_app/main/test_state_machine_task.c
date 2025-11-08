#include "unity.h"
#include "unity_fixture.h"

#include "file_system.h"
#include "gpio_task.h"
#include "wifi_task.h"
#include "aws_iot_task.h"
#include "state_machine_task.h"

// Test group setup
TEST_GROUP(state_machine_task);
TEST_SETUP(state_machine_task)
{
}

TEST_TEAR_DOWN(state_machine_task)
{
}

/** @brief Test: Initialize all tasks and clear WiFi credentials
 */
TEST(state_machine_task, initialize_tasks)
{
  file_system_init();
  gpio_task_init();
  wifi_task_init();

  file_system_clear(NVS_SSID_KEY);
  file_system_clear(NVS_PASSWORD_KEY);

  state_machine_task_init();

  vTaskDelay(pdMS_TO_TICKS(2000)); // Allow some time for initialization
}

/** @brief Test: State Machine initializes to STATE_PROVISIONING state when no WiFi credentials are stored
 */
TEST(state_machine_task, initialize_no_wifi_credentials)
{
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);
}

/** @brief Test: Set invalid WiFi credentials in PROVISIONING state */
TEST(state_machine_task, set_invalid_wifi_credentials_in_provisioning)
{
  // Simulate setting invalid WiFi credentials
  wifi_set_sta_credentials("invalid_ssid", "invalid_password");

  // Wait for WiFi connection attempt
  vTaskDelay(pdMS_TO_TICKS(20000));

  // Check the current state
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);
}

/** @brief Test: Set valid WiFi credentials in PROVISIONING state */
TEST(state_machine_task, set_valid_wifi_credentials_in_provisioning)
{
  // Simulate setting valid WiFi credentials
  wifi_set_sta_credentials("OctopusChurch", "BishopNemo");

  // Wait for WiFi connection
  vTaskDelay(pdMS_TO_TICKS(10000));

  // Check the current state
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_WIFI_CONNECTED, state);
}

/** @brief Test: Connect to AWS IoT after WiFi is connected */
TEST(state_machine_task, connect_to_aws_iot)
{
  aws_iot_task_init();
  // Wait for AWS IoT task to initialize
  vTaskDelay(pdMS_TO_TICKS(2000));

  // Check the current state
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_WIFI_CONNECTED, state);

  aws_iot_connect();

  // Wait for AWS IoT connection
  vTaskDelay(pdMS_TO_TICKS(10000));

  // Check the current state
  state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_AWS_IOT_CONNECTED, state);
}

/** @brief Test: Handle WiFi disconnection */
TEST(state_machine_task, wifi_disconnected)
{
  // Simulate WiFi disconnection
  wifi_set_sta_credentials("invalid_ssid", "invalid_password");

  // Wait for state machine to process the event
  vTaskDelay(pdMS_TO_TICKS(5000));

  // Check the current state
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_IDLE, state);
}

TEST_GROUP_RUNNER(state_machine_task)
{
  RUN_TEST_CASE(state_machine_task, initialize_tasks);
  RUN_TEST_CASE(state_machine_task, initialize_no_wifi_credentials);
  RUN_TEST_CASE(state_machine_task, set_invalid_wifi_credentials_in_provisioning);
  RUN_TEST_CASE(state_machine_task, set_valid_wifi_credentials_in_provisioning);
  RUN_TEST_CASE(state_machine_task, connect_to_aws_iot);
  RUN_TEST_CASE(state_machine_task, wifi_disconnected);
}
