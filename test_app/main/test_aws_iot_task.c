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
TEST(aws_iot_task, connect_to_broker_failed_invalid_broker)
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

/** @brief Test: Connect to AWS IoT broker with invalid client identifier
 *  @test Expected: Connection fails and appropriate state is maintained
 */
TEST(aws_iot_task, connect_to_broker_failed_invalid_client_id)
{
  ESP_LOGI(TAG, "Starting test: connect_to_broker_failed");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, NULL);
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

/** @brief Test: Subscribe to a topic on AWS IoT using invalid topic
 *  @test Expected: Subscription fails
 */
TEST(aws_iot_task, subscribe_to_topic_failed)
{
  ESP_LOGI(TAG, "Starting test: subscribe_to_topic_failed");

  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  aws_iot_subscribe_to_topic(NULL);
  vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_DELAY_MS)); // Allow time for subscription to process

  TEST_ASSERT_TRUE(aws_iot_is_connected());
  TEST_ASSERT_FALSE(aws_iot_is_listening());

  ESP_LOGI(TAG, "Get MQTT broker URL: %s", aws_iot_get_mqtt_broker_url());
  ESP_LOGI(TAG, "Get MQTT topic: %s", aws_iot_get_mqtt_topic());

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

  aws_iot_publish_button_event(500);
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

  aws_iot_publish_button_event(500);
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

/** @brief Test: Profile message round-trip time
 *  @test Expected: Measure time from publish -> PUBACK -> receive
 *  @details This test measures the following latencies:
 *    - Publish to PUBACK: Time for AWS IoT to acknowledge the message
 *    - PUBACK to Receive: Time for message to be delivered back to subscriber
 *    - Total Round-trip: Time from publish to receiving own message
 */
TEST(aws_iot_task, profile_message_round_trip)
{
  ESP_LOGI(TAG, "Starting test: profile_message_round_trip");

  // Connect to AWS IoT
  aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_connection(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT connected");

  // Subscribe to topic
  aws_iot_subscribe_to_topic(MQTT_TOPIC);
  vTaskDelay(pdMS_TO_TICKS(SUBSCRIBE_DELAY_MS));

  // Start listening
  aws_iot_start_listening();
  TEST_ASSERT_EQUAL(ESP_OK, aws_iot_wait_for_listening(CONNECT_TIMEOUT_MS));
  ESP_LOGI(TAG, "AWS IoT listening started");

  // Define sample size for profiling
  const int SAMPLE_SIZE = 10;
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "=== Running %d profiling iterations ===", SAMPLE_SIZE);
  
  unsigned long total_publish_to_puback = 0;
  unsigned long total_puback_to_receive = 0;
  unsigned long total_round_trip = 0;
  unsigned long min_publish_to_puback = 0xFFFFFFFF;
  unsigned long max_publish_to_puback = 0;
  unsigned long min_puback_to_receive = 0xFFFFFFFF;
  unsigned long max_puback_to_receive = 0;
  unsigned long min_round_trip = 0xFFFFFFFF;
  unsigned long max_round_trip = 0;

  for (int i = 0; i < SAMPLE_SIZE; i++)
  {
    // Reset and enable profiling
    aws_iot_reset_profiling();
    aws_iot_enable_profiling();

    aws_iot_publish_button_event(500);

    // Wait for round-trip to complete
    vTaskDelay(pdMS_TO_TICKS(10 * DELAY_TIME_MS));

    // Get profiling data
    AwsIotProfilingData_t profiling = aws_iot_get_profiling_data();

    // Verify all timestamps captured
    TEST_ASSERT_NOT_EQUAL(0, profiling.publish_timestamp_ms);
    TEST_ASSERT_NOT_EQUAL(0, profiling.puback_timestamp_ms);
    TEST_ASSERT_NOT_EQUAL(0, profiling.receive_timestamp_ms);
    TEST_ASSERT_NOT_EQUAL(0, profiling.publish_packet_id);

    // Calculate latencies for this iteration
    unsigned long publish_to_puback_ms = profiling.puback_timestamp_ms - profiling.publish_timestamp_ms;
    unsigned long puback_to_receive_ms = profiling.receive_timestamp_ms - profiling.puback_timestamp_ms;
    unsigned long total_round_trip_ms = profiling.receive_timestamp_ms - profiling.publish_timestamp_ms;

    // Verify event ordering (publish -> PUBACK -> receive)
    TEST_ASSERT_LESS_THAN(profiling.receive_timestamp_ms, profiling.puback_timestamp_ms);
    TEST_ASSERT_LESS_THAN(profiling.puback_timestamp_ms, profiling.publish_timestamp_ms);

    // Sanity check: latencies should be reasonable (< 10 seconds)
    TEST_ASSERT_LESS_THAN(10000, publish_to_puback_ms);
    TEST_ASSERT_LESS_THAN(10000, puback_to_receive_ms);
    TEST_ASSERT_LESS_THAN(10000, total_round_trip_ms);

    // Log iteration results
    ESP_LOGI(TAG, "Sample %2d: Publish->PUBACK=%3lu ms, PUBACK->Receive=%3lu ms, Total=%3lu ms (Packet ID: %u)",
             i + 1, publish_to_puback_ms, puback_to_receive_ms, total_round_trip_ms, profiling.publish_packet_id);

    // Accumulate statistics
    total_publish_to_puback += publish_to_puback_ms;
    total_puback_to_receive += puback_to_receive_ms;
    total_round_trip += total_round_trip_ms;

    // Track min/max for publish to PUBACK
    if (publish_to_puback_ms < min_publish_to_puback)
    {
      min_publish_to_puback = publish_to_puback_ms;
    }
    if (publish_to_puback_ms > max_publish_to_puback)
    {
      max_publish_to_puback = publish_to_puback_ms;
    }

    // Track min/max for PUBACK to receive
    if (puback_to_receive_ms < min_puback_to_receive)
    {
      min_puback_to_receive = puback_to_receive_ms;
    }
    if (puback_to_receive_ms > max_puback_to_receive)
    {
      max_puback_to_receive = puback_to_receive_ms;
    }

    // Track min/max for total round-trip
    if (total_round_trip_ms < min_round_trip)
    {
      min_round_trip = total_round_trip_ms;
    }
    if (total_round_trip_ms > max_round_trip)
    {
      max_round_trip = total_round_trip_ms;
    }
  }

  // Calculate averages
  unsigned long avg_publish_to_puback = total_publish_to_puback / SAMPLE_SIZE;
  unsigned long avg_puback_to_receive = total_puback_to_receive / SAMPLE_SIZE;
  unsigned long avg_round_trip = total_round_trip / SAMPLE_SIZE;

  // Log statistical summary
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "=== Profiling Summary (%d samples) ===", SAMPLE_SIZE);
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Publish -> PUBACK Latency:");
  ESP_LOGI(TAG, "  Average: %lu ms", avg_publish_to_puback);
  ESP_LOGI(TAG, "  Min:     %lu ms", min_publish_to_puback);
  ESP_LOGI(TAG, "  Max:     %lu ms", max_publish_to_puback);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "PUBACK -> Receive Latency:");
  ESP_LOGI(TAG, "  Average: %lu ms", avg_puback_to_receive);
  ESP_LOGI(TAG, "  Min:     %lu ms", min_puback_to_receive);
  ESP_LOGI(TAG, "  Max:     %lu ms", max_puback_to_receive);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Total Round-trip Latency:");
  ESP_LOGI(TAG, "  Average: %lu ms", avg_round_trip);
  ESP_LOGI(TAG, "  Min:     %lu ms", min_round_trip);
  ESP_LOGI(TAG, "  Max:     %lu ms", max_round_trip);
  ESP_LOGI(TAG, "========================================");

  // Verify message statistics are valid
  AwsIotStatistics_t stats = aws_iot_get_statistics();
  TEST_ASSERT_EQUAL(SAMPLE_SIZE, stats.messages_published);
  TEST_ASSERT_EQUAL(SAMPLE_SIZE, stats.messages_received);
}

TEST_GROUP_RUNNER(aws_iot_task)
{
  RUN_TEST_CASE(aws_iot_task, initial_state);
  RUN_TEST_CASE(aws_iot_task, connect_to_broker_failed_invalid_broker);
  RUN_TEST_CASE(aws_iot_task, connect_to_broker_failed_invalid_client_id);
  RUN_TEST_CASE(aws_iot_task, connect_to_broker);
  RUN_TEST_CASE(aws_iot_task, subscribe_to_topic_failed);
  RUN_TEST_CASE(aws_iot_task, subscribe_to_topic);
  RUN_TEST_CASE(aws_iot_task, start_listening_for_messages);
  RUN_TEST_CASE(aws_iot_task, publish_button_events);
  RUN_TEST_CASE(aws_iot_task, profile_message_round_trip);
}