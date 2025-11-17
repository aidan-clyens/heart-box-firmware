#include "unity.h"
#include "unity_fixture.h"

// Include the component header
#include "aws_iot_task.h"
#include "wifi_task.h"

#include "application.h"

#include "esp_log.h"

#define MQTT_CLIENT_IDENTIFIER "Heart_Box_1"
// #ifndef MQTT_CLIENT_IDENTIFIER
// #error "MQTT_CLIENT_IDENTIFIER must be defined in sdkconfig"
// #endif // MQTT_CLIENT_IDENTIFIER
#define MQTT_CLIENT_IDENTIFIER_LENGTH ((uint16_t)(sizeof(MQTT_CLIENT_IDENTIFIER) - 1))

#define MQTT_BROKER_ENDPOINT "airgahux2exxu-ats.iot.us-east-1.amazonaws.com"
// #ifndef MQTT_BROKER_ENDPOINT
// #error "MQTT_BROKER_ENDPOINT must be defined in sdkconfig"
// #endif // MQTT_BROKER_ENDPOINT

#define MQTT_TOPIC "heartbox/1"
// #ifndef MQTT_TOPIC
// #error "MQTT_TOPIC must be defined in sdkconfig"
// #endif // MQTT_TOPIC
#define MQTT_TOPIC_LENGTH ((uint16_t)(sizeof(MQTT_TOPIC) - 1))

#define DELAY_TIME_MS 500
#define CONNECT_TIMEOUT_MS 20 * 1000
#define SUBSCRIBE_DELAY_MS 5 * 1000
#define HEAP_TOLERANCE_BYTES 900 // Allow for ESP-IDF WiFi/TLS stack one-time initialization overhead

static const char *TAG = "TEST_AWS_IOT_TASK";

static size_t initial_free_heap_size = 0;

// Test group setup
TEST_GROUP(aws_iot_task);
TEST_SETUP(aws_iot_task)
{
  initial_free_heap_size = esp_get_free_heap_size();
  ESP_LOGI(TAG, "Initial free heap size: %u bytes", initial_free_heap_size);

  TEST_ASSERT_EQUAL(ESP_OK, application_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS)); // Allow time for task to initialize

  wifi_set_sta_credentials(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  TEST_ASSERT_EQUAL(ESP_OK, wifi_wait_for_connection(CONNECT_TIMEOUT_MS));
}

TEST_TEAR_DOWN(aws_iot_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, application_deinit());
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

/** @brief Test: Initial state of AWS IoT task
 *  @test Expected: AWS IoT task initializes successfully
 */
TEST(aws_iot_task, initial_state)
{
  ESP_LOGI(TAG, "Starting test: initial_state");

  TEST_ASSERT_FALSE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());

  TEST_ASSERT_EQUAL_STRING("", aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING("", aws_iot_get_mqtt_topic());
}

/** @brief Test: Connect to AWS IoT broker with invalid URL
 *  @test Expected: Connection fails and appropriate state is maintained
 */
TEST(aws_iot_task, connect_to_broker_failed)
{
  ESP_LOGI(TAG, "Starting test: connect_to_broker_failed");

  aws_iot_connect("https://invalid-url", MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));

  TEST_ASSERT_FALSE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());

  TEST_ASSERT_EQUAL_STRING("", aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING("", aws_iot_get_mqtt_topic());

  // TODO - Verify cleanup of network context
}

/** @brief Test: Connect to AWS IoT broker
 *  @test Expected: Successful connection and listening state
 */
TEST(aws_iot_task, connect_to_broker)
{
  ESP_LOGI(TAG, "Starting test: connect_to_broker");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());

  TEST_ASSERT_EQUAL_STRING(MQTT_BROKER_ENDPOINT, aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING("", aws_iot_get_mqtt_topic());
}

/** @brief Test: Subscribe to a topic on AWS IoT
 *  @test Expected: Successful subscription
 */
TEST(aws_iot_task, subscribe_to_topic)
{
  ESP_LOGI(TAG, "Starting test: subscribe_to_topic");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  aws_iot_subscribe_to_topic(MQTT_TOPIC);
  vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_DELAY_MS)); // Allow time for subscription to process

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());

  ESP_LOGI(TAG, "Get MQTT broker URL: %s", aws_iot_get_mqtt_broker_url());
  ESP_LOGI(TAG, "Get MQTT topic: %s", aws_iot_get_mqtt_topic());

  TEST_ASSERT_EQUAL_STRING(MQTT_BROKER_ENDPOINT, aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING(MQTT_TOPIC, aws_iot_get_mqtt_topic());
}

/** @brief Test: Start listening for messages from AWS IoT broker
 *  @test Expected: Successful listening state
 */
TEST(aws_iot_task, start_listening_for_messages)
{
  ESP_LOGI(TAG, "Starting test: start_listening_for_messages");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  aws_iot_subscribe_to_topic(MQTT_TOPIC);
  vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_DELAY_MS)); // Allow time for subscription to process

  aws_iot_start_listening();
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_listening(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT listening started");

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_TRUE(aws_iot_is_listening());

  TEST_ASSERT_EQUAL_STRING(MQTT_BROKER_ENDPOINT, aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING(MQTT_TOPIC, aws_iot_get_mqtt_topic());

  AwsIotStatistics_t stats = aws_iot_get_statistics();
  ESP_LOGI(TAG, "AWS IoT statistics: connections attempts=%u, successful connections=%u, published=%u, received=%u",
           stats.connection_attempts,
           stats.successful_connections,
           stats.messages_published,
           stats.messages_received);
  TEST_ASSERT_EQUAL(1, stats.connection_attempts);
  TEST_ASSERT_EQUAL(1, stats.successful_connections);
  TEST_ASSERT_EQUAL(0, stats.messages_published);
  TEST_ASSERT_EQUAL(0, stats.messages_received);
}

/** @brief Test: Publish button pressed event to AWS IoT broker
 *  @test Expected: Successful publish and statistics update
 */
TEST(aws_iot_task, publish_button_events)
{
  ESP_LOGI(TAG, "Starting test: publish_button_events");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  aws_iot_subscribe_to_topic(MQTT_TOPIC);
  vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_DELAY_MS)); // Allow time for subscription to process

  aws_iot_start_listening();
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_listening(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT listening started");
  
  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_TRUE(aws_iot_is_listening());

  TEST_ASSERT_EQUAL_STRING(MQTT_BROKER_ENDPOINT, aws_iot_get_mqtt_broker_url());
  TEST_ASSERT_EQUAL_STRING(MQTT_TOPIC, aws_iot_get_mqtt_topic());

  AwsIotStatistics_t stats = aws_iot_get_statistics();
  ESP_LOGI(TAG, "AWS IoT statistics before publishing: connections attempts=%u, successful connections=%u, published=%u, received=%u",
           stats.connection_attempts,
           stats.successful_connections,
           stats.messages_published,
           stats.messages_received);

  TEST_ASSERT_EQUAL(1, stats.connection_attempts);
  TEST_ASSERT_EQUAL(1, stats.successful_connections);
  TEST_ASSERT_EQUAL(0, stats.messages_published);
  TEST_ASSERT_EQUAL(0, stats.messages_received);

  aws_iot_publish_button_pressed_event();
  vTaskDelay(pdMS_TO_TICKS(10 * DELAY_TIME_MS)); // Allow time for message to be published

  stats = aws_iot_get_statistics();
  ESP_LOGI(TAG, "AWS IoT statistics after publishing: connections attempts=%u, successful connections=%u, published=%u, received=%u",
           stats.connection_attempts,
           stats.successful_connections,
           stats.messages_published,
           stats.messages_received);

  TEST_ASSERT_EQUAL(1, stats.connection_attempts);
  TEST_ASSERT_EQUAL(1, stats.successful_connections);
  TEST_ASSERT_EQUAL(1, stats.messages_published);
  TEST_ASSERT_EQUAL(1, stats.messages_received);

  aws_iot_publish_button_released_event();
  vTaskDelay(pdMS_TO_TICKS(10 * DELAY_TIME_MS)); // Allow time for message to be published

  stats = aws_iot_get_statistics();
  ESP_LOGI(TAG, "AWS IoT statistics after publishing: connections attempts=%u, successful connections=%u, published=%u, received=%u",
           stats.connection_attempts,
           stats.successful_connections,
           stats.messages_published,
           stats.messages_received);

  TEST_ASSERT_EQUAL(1, stats.connection_attempts);
  TEST_ASSERT_EQUAL(1, stats.successful_connections);
  TEST_ASSERT_EQUAL(2, stats.messages_published);
  TEST_ASSERT_EQUAL(2, stats.messages_received);
}

TEST_GROUP_RUNNER(aws_iot_task)
{
  RUN_TEST_CASE(aws_iot_task, initial_state);
  RUN_TEST_CASE(aws_iot_task, connect_to_broker_invalid_url);
  RUN_TEST_CASE(aws_iot_task, connect_to_broker);
  RUN_TEST_CASE(aws_iot_task, subscribe_to_topic);
  RUN_TEST_CASE(aws_iot_task, start_listening_for_messages);
  RUN_TEST_CASE(aws_iot_task, publish_button_events);
}