#include "unity.h"
#include "unity_fixture.h"

#include "script_helpers.h"

// Include the component header
#include "wifi_task.h"
#include "file_system.h"

#ifndef TEST_WIFI_SSID
#error "TEST_WIFI_SSID is not defined. Please define it in the project configuration."
#endif

#ifndef TEST_WIFI_PASSWORD
#error "TEST_WIFI_PASSWORD is not defined. Please define it in the project configuration."
#endif

#define DELAY_TIME_MS 500
#define CONNECT_TIMEOUT_MS 30 * 1000

static void wait_for_connection(unsigned int timeout_ms)
{
  TickType_t start_tick = xTaskGetTickCount();
  while (xTaskGetTickCount() - start_tick < pdMS_TO_TICKS(timeout_ms))
  {
    if (wifi_task_is_connected())
    {
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  TEST_FAIL_MESSAGE("Timeout waiting for WiFi connection state change");
}

// Test group setup
TEST_GROUP(wifi_task);
TEST_SETUP(wifi_task)
{
  file_system_init();
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to initialize
}

TEST_TEAR_DOWN(wifi_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_deinit());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to stop
}

/** @brief Test: Initialize WiFi task
 *  @test Expected: WiFi task starts in idle state (not started, not connected)
 */
TEST(wifi_task, initialize_task)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());
}

/** @brief Test: Start WiFi in AP mode
 *  @test Expected: WiFi starts in AP mode successfully
 */
TEST(wifi_task, start_ap_mode)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_ap_mode();

  vTaskDelay(pdMS_TO_TICKS(CONNECT_TIMEOUT_MS)); // Allow some time for command to process and start

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_AP, mode);
}

/** @brief Test: Start WiFi in STA mode with invalid credentials
 *  @test Expected: WiFi fails to connect in STA mode
 */
TEST(wifi_task, start_sta_mode_invalid_credentials)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials("TestSSID", "TestPassword");

  vTaskDelay(pdMS_TO_TICKS(CONNECT_TIMEOUT_MS)); // Allow some time for command to process and start

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING("TestSSID", creds.ssid);
  TEST_ASSERT_EQUAL_STRING("TestPassword", creds.password);
}

/** @brief Test: Start WiFi in STA mode with valid credentials
 *  @test Expected: WiFi connects successfully in STA mode
 */
TEST(wifi_task, start_sta_mode_valid_credentials)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  wait_for_connection(CONNECT_TIMEOUT_MS);

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);
}

TEST(wifi_task, ping)
{
}

/** @brief Test: Change WiFi from STA mode to AP mode
 *  @test Expected: WiFi stops STA mode and starts in AP mode successfully
 */
TEST(wifi_task, change_to_ap_mode)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  wait_for_connection(CONNECT_TIMEOUT_MS);

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);

  // Now switch to AP mode
  wifi_set_ap_mode();

  vTaskDelay(pdMS_TO_TICKS(CONNECT_TIMEOUT_MS)); // Allow some time for command to process and start

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_AP, mode);
}

TEST_GROUP_RUNNER(wifi_task)
{
  RUN_TEST_CASE(wifi_task, initialize_task);
  RUN_TEST_CASE(wifi_task, start_ap_mode);
  RUN_TEST_CASE(wifi_task, start_sta_mode_invalid_credentials);
  RUN_TEST_CASE(wifi_task, start_sta_mode_valid_credentials);
  // RUN_TEST_CASE(wifi_task, ping);
  RUN_TEST_CASE(wifi_task, change_to_ap_mode);
}
