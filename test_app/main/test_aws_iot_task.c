#include "unity.h"
#include "unity_fixture.h"

#include "script_helpers.h"

// Include the component header
#include "aws_iot_task.h"
#include "wifi_task.h"

#define DELAY_TIME_MS 500
#define CONNECT_TIMEOUT_MS 30 * 1000

// Test group setup
TEST_GROUP(aws_iot_task);
TEST_SETUP(aws_iot_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_init());
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_task_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to initialize

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  wait_for_connection(CONNECT_TIMEOUT_MS, true);
}

TEST_TEAR_DOWN(aws_iot_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_task_deinit());
  TEST_ASSERT_EQUAL(ESP_OK, wifi_task_deinit());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to stop
}

/** @brief Test: Initial state of AWS IoT task
 *  @test Expected: AWS IoT task initializes successfully
 */
TEST(aws_iot_task, initial_state)
{
  TEST_ASSERT_FALSE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());
}

/** @brief Test: Connect to AWS IoT broker
 *  @test Expected: Successful connection and listening state
 */
TEST(aws_iot_task, connect_to_broker)
{
  aws_iot_connect();

  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());
}

/** @brief Test: Start listening for messages from AWS IoT broker
 *  @test Expected: Successful listening state
 */
TEST(aws_iot_task, start_listening_for_messages)
{
  aws_iot_connect();
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));

  aws_iot_start_listening();
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Wait for listening to start

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_TRUE(aws_iot_is_listening());

  AwsIotStatistics_t stats = aws_iot_get_statistics();
  TEST_ASSERT_EQUAL(1, stats.connection_attempts);
  TEST_ASSERT_EQUAL(1, stats.successful_connections);
  TEST_ASSERT_EQUAL(0, stats.messages_published);
  TEST_ASSERT_EQUAL(0, stats.messages_received);
}

TEST_GROUP_RUNNER(aws_iot_task)
{
  RUN_TEST_CASE(aws_iot_task, initial_state);
  // RUN_TEST_CASE(aws_iot_task, connect_to_broker);
  // RUN_TEST_CASE(aws_iot_task, start_listening_for_messages);
}
