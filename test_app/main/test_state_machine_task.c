#include "unity.h"
#include "unity_fixture.h"

#include "file_system.h"
#include "gpio_task.h"
#include "wifi_task.h"
#include "aws_iot_task.h"
#include "state_machine_task.h"

#ifndef TEST_WIFI_SSID
#error "TEST_WIFI_SSID must be defined for these tests to run"
#endif

#ifndef TEST_WIFI_PASSWORD
#error "TEST_WIFI_PASSWORD must be defined for these tests to run"
#endif

#ifndef MQTT_BROKER_ENDPOINT
#error "MQTT_BROKER_ENDPOINT must be defined in sdkconfig"
#endif

#define DELAY_TIME_MS 500
#define CONNECT_TIMEOUT_MS 20000

// Test group setup
TEST_GROUP(state_machine_task);
TEST_SETUP(state_machine_task)
{
  // Clear WiFi credentials for clean state
  file_system_clear(NVS_SSID_KEY);
  file_system_clear(NVS_PASSWORD_KEY);
  
  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_init());
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_init());
  TEST_ASSERT_EQUAL(ESP_OK, state_machine_task_init());

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to initialize
}

TEST_TEAR_DOWN(state_machine_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, state_machine_task_deinit());
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_deinit());
  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_deinit());

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for tasks to stop
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
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);

  // Simulate setting invalid WiFi credentials
  wifi_set_sta_credentials("invalid_ssid", "invalid_password");

  // Wait for WiFi connection attempt
  wifi_wait_for_connection(CONNECT_TIMEOUT_MS);

  // Check the current state
  state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);
}

/** @brief Test: Set valid WiFi credentials in PROVISIONING state */
TEST(state_machine_task, set_valid_wifi_credentials_in_provisioning)
{
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);

  // Simulate setting valid WiFi credentials
  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  // Wait for WiFi connection
  wifi_wait_for_connection(CONNECT_TIMEOUT_MS);

  // Check the current state
  state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_WIFI_CONNECTED, state);
}

/** @brief Test: Connect to AWS IoT after WiFi is connected */
TEST(state_machine_task, connect_to_aws_iot)
{
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_task_init());
  // Wait for AWS IoT task to initialize
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_PROVISIONING, state);

  // Simulate setting valid WiFi credentials
  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  // Wait for WiFi connection
  wifi_wait_for_connection(CONNECT_TIMEOUT_MS);

  // Check the current state
  state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_WIFI_CONNECTED, state);

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);

  // Wait for AWS IoT connection
  wifi_wait_for_connection(CONNECT_TIMEOUT_MS);

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
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // Check the current state
  eAppState_t state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_IDLE, state);

  // Wait for reconnection attempts
  vTaskDelay(pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));

  // Check the current state
  state = state_machine_get_current_app_state();
  TEST_ASSERT_EQUAL(STATE_AWS_IOT_CONNECTED, state);
}

TEST_GROUP_RUNNER(state_machine_task)
{
  RUN_TEST_CASE(state_machine_task, initialize_no_wifi_credentials);
  RUN_TEST_CASE(state_machine_task, set_invalid_wifi_credentials_in_provisioning);
  RUN_TEST_CASE(state_machine_task, set_valid_wifi_credentials_in_provisioning);
  RUN_TEST_CASE(state_machine_task, connect_to_aws_iot);
  // RUN_TEST_CASE(state_machine_task, wifi_disconnected);
}
