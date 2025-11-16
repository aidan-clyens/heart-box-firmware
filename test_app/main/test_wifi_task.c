#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"
#include "esp_heap_caps.h"

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
#define HEAP_TOLERANCE_BYTES 600 // Allow for ESP-IDF WiFi stack one-time initialization overhead (~288 bytes)

static const char *TAG = "TEST_WIFI_TASK";

static size_t initial_free_heap_size = 0;

// Test group setup
TEST_GROUP(wifi_task);
TEST_SETUP(wifi_task)
{
  initial_free_heap_size = esp_get_free_heap_size();
  ESP_LOGI(TAG, "Initial free heap size: %u bytes", initial_free_heap_size);

  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to initialize
}

TEST_TEAR_DOWN(wifi_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_deinit());
  vTaskDelay(pdMS_TO_TICKS(1000)); // Allow time for task to stop and cleanup to complete

  // Check heap for memory leaks
  size_t final_free_heap_size = esp_get_free_heap_size();
  int heap_diff = final_free_heap_size - initial_free_heap_size;
  
  ESP_LOGI(TAG, "Heap difference: %d bytes (tolerance: ±%d)", heap_diff, HEAP_TOLERANCE_BYTES);

  if (heap_diff < -HEAP_TOLERANCE_BYTES)
  {
    ESP_LOGE(TAG, "Memory leak detected! Lost %d bytes", -heap_diff);
  }
  else if (heap_diff > HEAP_TOLERANCE_BYTES)
  {
    ESP_LOGW(TAG, "Heap grew unexpectedly by %d bytes", heap_diff);
  }

  TEST_ASSERT_GREATER_OR_EQUAL_INT(-HEAP_TOLERANCE_BYTES, heap_diff);
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

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow some time for command to process and start

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

  // Wait for timeout BUT before WiFi stops itself
  vTaskDelay(pdMS_TO_TICKS(3000)); // Less than full retry cycle

  TEST_ASSERT_TRUE(wifi_task_is_started()); // Still trying
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING("TestSSID", creds.ssid);
  TEST_ASSERT_EQUAL_STRING("TestPassword", creds.password);

  // Wait long enough for all retries to fail
  const unsigned int retry_timeout_ms = (MAX_WIFI_CONNECT_RETRIES + 1) * 6000;
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, wifi_wait_for_connection(retry_timeout_ms));

  vTaskDelay(pdMS_TO_TICKS(1000));

  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());
}

/** @brief Test: Start WiFi in STA mode with valid credentials
 *  @test Expected: WiFi connects successfully in STA mode
 */
TEST(wifi_task, start_sta_mode_valid_credentials)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);
}

/** @brief Test: Send a ping command to an external host
 *  @test Expected: Ping command is sent successfully
 */
TEST(wifi_task, ping)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);

  // Send a ping to a known host
  wifi_ping("8.8.8.8");

  // Allow some time for ping to complete
  vTaskDelay(pdMS_TO_TICKS(5000));

  TEST_IGNORE_MESSAGE("Ping test not yet implemented");
}

/** @brief Test: Change WiFi from STA mode to AP mode
 *  @test Expected: WiFi stops STA mode and starts in AP mode successfully
 */
TEST(wifi_task, change_from_sta_to_ap_mode)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));

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

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow some time for command to process and start

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_AP, mode);
}

/** @brief Test: Change WiFi from AP mode to STA mode
 *  @test Expected: WiFi stops AP mode and starts in STA mode successfully
 */
TEST(wifi_task, change_from_ap_mode_sta_mode)
{
  TEST_ASSERT_FALSE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_set_ap_mode();

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow some time for command to process and start

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_FALSE(wifi_task_is_connected());

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_AP, mode);

  // Now switch to STA mode
  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  // Get the current credentials and verify
  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);
}

/** @brief Test: Change credentials while connected
 *  @test Expected: WiFi disconnects from old network and connects to new one
 */
TEST(wifi_task, change_credentials_while_connected)
{
  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));
  TEST_ASSERT_TRUE(wifi_task_is_connected());

  // Change to different network (or same with different password to test disconnect)
  wifi_set_sta_credentials("DifferentSSID", "DifferentPassword");

  vTaskDelay(pdMS_TO_TICKS(2000));
  TEST_ASSERT_FALSE(wifi_task_is_connected()); // Should disconnect from old network
}

/** @brief Test: Rapidly switch between AP and STA modes
 *  @test Expected: WiFi handles rapid mode changes without crashing
 */
TEST(wifi_task, rapid_mode_switching)
{
  for (int i = 0; i < 3; i++)
  {
    wifi_set_ap_mode();
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_TRUE(wifi_task_is_started());

    wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_TRUE(wifi_task_is_started());
  }
  
  // Final state should be STA mode
  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));

  wifi_mode_t mode = wifi_get_mode();
  TEST_ASSERT_EQUAL(WIFI_MODE_STA, mode);

  WifiCredentials_t creds = wifi_get_current_credentials();
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, creds.ssid);
  TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, creds.password);

  TEST_ASSERT_TRUE(wifi_task_is_started());
  TEST_ASSERT_TRUE(wifi_task_is_connected());
}

TEST_GROUP_RUNNER(wifi_task)
{
  RUN_TEST_CASE(wifi_task, initialize_task);
  RUN_TEST_CASE(wifi_task, start_ap_mode);
  RUN_TEST_CASE(wifi_task, start_sta_mode_invalid_credentials);
  RUN_TEST_CASE(wifi_task, start_sta_mode_valid_credentials);
  RUN_TEST_CASE(wifi_task, ping);
  RUN_TEST_CASE(wifi_task, change_from_sta_to_ap_mode);
  RUN_TEST_CASE(wifi_task, change_from_ap_mode_sta_mode);
  RUN_TEST_CASE(wifi_task, change_credentials_while_connected);
  RUN_TEST_CASE(wifi_task, rapid_mode_switching);
}
