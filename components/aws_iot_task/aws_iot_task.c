#include "aws_iot_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

/* MQTT API headers. */
#include "core_mqtt.h"
#include "core_mqtt_state.h"

/* OpenSSL sockets transport implementation. */
#include "network_transport.h"

#include "clock.h"

#include "generic_task.h"
#include "message_types.h"
#include "state_machine_task.h"

#include "cJSON.h"

#define MQTT_AWS_IOT_TASK_STACK_SIZE 4096
#define MQTT_KEEP_ALIVE_TASK_STACK_SIZE 4096

#define MQTT_PORT 8883

// Time definitions
#define MQTT_KEEP_ALIVE_INTERVAL_S 60U
#define MQTT_KEEP_ALIVE_TIMER_INTERVAL_MS (MQTT_KEEP_ALIVE_INTERVAL_S * 1000U)
#define MQTT_CONNACK_RECV_TIMEOUT_MS 5000U
#define MQTT_PROCESS_LOOP_TIMEOUT_MS 5000U

#define MQTT_OUTGOING_PUBLISH_RECORD_LEN 30U
#define MQTT_INCOMING_PUBLISH_RECORD_LEN 30U

#define MQTT_NETWORK_BUFFER_SIZE 2048

#define MQTT_SUBACK_RECEIVED_BIT (1 << 0)
#define MQTT_SUBACK_SUCCESS_BIT (1 << 1)
#define MQTT_PUBACK_RECEIVED_BIT (1 << 2)

extern const char root_cert_auth_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const char root_cert_auth_end[] asm("_binary_AmazonRootCA1_pem_end");

extern const char client_cert_start[] asm("_binary_device_certificate_pem_crt_start");
extern const char client_cert_end[] asm("_binary_device_certificate_pem_crt_end");

extern const char client_key_start[] asm("_binary_private_key_pem_key_start");
extern const char client_key_end[] asm("_binary_private_key_pem_key_end");

// --- Module State ---
static const char *TAG_AWS_IOT = "AWS_IOT_TASK";
static const char *TAG_AWS_IOT_KEEP_ALIVE = "AWS_IOT_KEEP_ALIVE_TASK";
static GenericTask *aws_iot_task = NULL;

static const char *MSG_CLIENT_TAG = "client";
static const char *MSG_BUTTON_TAG = "button";
static const char *MSG_LOG_TAG = "log";
static const char *MSG_DURATION_MS_TAG = "duration_ms";
static const char *MSG_BUTTON_PRESSED = "pressed";
static const char *MSG_BUTTON_RELEASED = "released";

// MQTT connection
static MQTTContext_t mqtt_context = {0};
static MQTTConnectInfo_t connect_info = {0};
static char *mqtt_client_identifier = NULL;
static char *mqtt_broker_url = NULL;

// Network
static NetworkContext_t network_context = {0};
static uint8_t buffer[MQTT_NETWORK_BUFFER_SIZE];
static StaticSemaphore_t tls_context_semaphore;

// MQTT subscriptions
static MQTTSubscribeInfo_t subscription_list[1];
static unsigned short subscribe_packet_identifier = 0U;
static char *mqtt_topic = NULL;
static char *mqtt_log_topic = NULL;

// MQTT publish tracking
static uint16_t expected_puback_packet_id = 0;

// MQTT message queues
static MQTTPubAckInfo_t outgoing_publish_records[MQTT_OUTGOING_PUBLISH_RECORD_LEN];
static MQTTPubAckInfo_t incoming_publish_records[MQTT_INCOMING_PUBLISH_RECORD_LEN];

// Events handling
static bool session_present = false;
static EventGroupHandle_t mqtt_event_group = NULL;
static SemaphoreHandle_t mqtt_process_mutex = NULL;

static TaskHandle_t keep_alive_task_handle = NULL;
static volatile bool keep_alive_should_stop = false;

static AwsIotStatistics_t aws_iot_statistics = {0};
static AwsIotProfilingData_t aws_iot_profiling_data = {0};

/** @brief Post a command message to the AWS IoT task
 *  @param msg The message to post
 */
static BaseType_t aws_iot_post_msg(AwsIotMsg_t msg)
{
  if (aws_iot_task == NULL)
  {
    return pdFALSE;
  }
  return generic_task_post_msg(aws_iot_task, &msg, sizeof(AwsIotMsg_t));
}

/** @brief Public API: Request a connection to the configured AWS IoT broker
 *  @param broker_url The AWS IoT broker URL
 *  @param client_identifier The MQTT client identifier
 */
void aws_iot_connect(const char *broker_url, const char *client_identifier)
{
  if (broker_url == NULL || client_identifier == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot connect to AWS IoT with NULL broker URL or client identifier");
    return;
  }

  AwsIotConnectData_t connect_data;
  strncpy(connect_data.broker_url, broker_url, MAX_URL_LEN);
  strncpy(connect_data.client_identifier, client_identifier, MAX_CLIENT_IDENTIFIER_LEN);

  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_CONNECT;
  msg.data.connect_data = connect_data;
  aws_iot_post_msg(msg);
}

/** @brief Public API: Subscribe to a topic on AWS IoT */
void aws_iot_subscribe_to_topic(const char *topic)
{
  if (topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot subscribe to NULL topic");
    return;
  }

  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_SUBSCRIBE;
  strncpy(msg.data.topic, topic, MAX_HOSTNAME_LEN);
  aws_iot_post_msg(msg);
}

/** @brief Public API: Start listening for incoming MQTT messages from AWS IoT */
void aws_iot_start_listening(void)
{
  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_START_LISTENING;
  aws_iot_post_msg(msg);
}

/** @brief Public API: Publish a button event to AWS IoT */
void aws_iot_publish_button_event(unsigned int duration_ms)
{
  const char *client_id = aws_iot_get_mqtt_client_identifier();
  if (client_id == NULL || client_id[0] == '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot publish button event without valid MQTT client identifier");
    return;
  }

  if (duration_ms == 0)
  {
    ESP_LOGW(TAG_AWS_IOT, "Button event duration is 0 ms, not publishing");
    return;
  }

  AwsIotMsg_t msg = {
    .type = APP_AWS_IOT_CMD_PUBLISH_BUTTON_EVENT,
    .data = {
      .button_event = {
        .duration_ms = duration_ms
      }
    }
  };

  strncpy(msg.data.button_event.client_identifier, client_id, MAX_CLIENT_IDENTIFIER_LEN);
  aws_iot_post_msg(msg);
}

/** @brief Public API: Get the current MQTT connection status
 *  @return Current MQTT connection status
 */
bool aws_iot_is_connected(void)
{
  return (mqtt_context.connectStatus == MQTTConnected);
}

/** @brief Public API: Check if AWS IoT task is listening for messages
 *  @return true if listening, false otherwise
 */
bool aws_iot_is_listening(void)
{
  return (keep_alive_task_handle != NULL);
}

/** @brief: Public API: Wait for AWS IoT connection with timeout
 *  @param timeout_ms Maximum time to wait in milliseconds
 *  @return ESP_OK if connected, ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t aws_iot_wait_for_connection(unsigned int timeout_ms)
{
  const TickType_t start_time = xTaskGetTickCount();
  const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  while ((xTaskGetTickCount() - start_time) < timeout_ticks)
  {
    if (aws_iot_is_connected())
    {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  return ESP_ERR_TIMEOUT;
}

/** @brief: Public API: Wait for AWS IoT listening state with timeout
 *  @param timeout_ms Maximum time to wait in milliseconds
 *  @return ESP_OK if listening, ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t aws_iot_wait_for_listening(unsigned int timeout_ms)
{
  const TickType_t start_time = xTaskGetTickCount();
  const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  while ((xTaskGetTickCount() - start_time) < timeout_ticks)
  {
    if (aws_iot_is_listening())
    {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  return ESP_ERR_TIMEOUT;
}

/** @brief Public API: Get the current MQTT client identifier
 *  @return Current MQTT client identifier string
 */
const char *aws_iot_get_mqtt_client_identifier(void)
{
  if (mqtt_client_identifier == NULL || mqtt_client_identifier[0] == '\0')
  {
    return "";
  }

  return mqtt_client_identifier;
}

/** @brief Public API: Get the current MQTT broker URL
 *  @return Current MQTT broker URL string
 */
const char *aws_iot_get_mqtt_broker_url(void)
{
  if (mqtt_broker_url == NULL || mqtt_broker_url[0] == '\0')
  {
    return "";
  }

  return mqtt_broker_url;
}

/** @brief Public API: Get the current MQTT topic
 *  @return Current MQTT topic string
 */
const char *aws_iot_get_mqtt_topic(void)
{
  if (mqtt_topic == NULL || mqtt_topic[0] == '\0')
  {
    return "";
  }

  return mqtt_topic;
}

/** @brief Public API: Set the MQTT log topic
 *  @param topic The MQTT log topic name to set
 */
void aws_iot_set_mqtt_log_topic(const char *topic)
{
  if (topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot set MQTT log topic to NULL");
    return;
  }

  if (strlen(topic) >= MAX_HOSTNAME_LEN)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT log topic length exceeds maximum allowed: %d", MAX_HOSTNAME_LEN - 1);
    return;
  }

  mqtt_log_topic = (char *)malloc(MAX_HOSTNAME_LEN);
  if (mqtt_log_topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to allocate memory for MQTT log topic");
    return;
  }

  strncpy(mqtt_log_topic, topic, MAX_HOSTNAME_LEN - 1);
  mqtt_log_topic[MAX_HOSTNAME_LEN - 1] = '\0';

  ESP_LOGI(TAG_AWS_IOT, "MQTT log topic set to: %s", mqtt_log_topic);
}

/** @brief Public API: Get the current MQTT log topic
 *  @return Current MQTT log topic string
 */
const char *aws_iot_get_mqtt_log_topic(void)
{
  if (mqtt_log_topic == NULL || mqtt_log_topic[0] == '\0')
  {
    return "";
  }

  return mqtt_log_topic;
}

/** @brief Public API: Retrieve AWS IoT task statistics
 *  @return Structure containing AWS IoT task statistics
 */
AwsIotStatistics_t aws_iot_get_statistics(void)
{
  return aws_iot_statistics;
}

/** @brief Public API: Enable profiling for the next message publish */
void aws_iot_enable_profiling(void)
{
  aws_iot_profiling_data.profiling_active = true;
  aws_iot_profiling_data.publish_timestamp_ms = 0;
  aws_iot_profiling_data.puback_timestamp_ms = 0;
  aws_iot_profiling_data.receive_timestamp_ms = 0;
  aws_iot_profiling_data.publish_packet_id = 0;
  ESP_LOGI(TAG_AWS_IOT, "Profiling enabled for next message publish");
}

/** @brief Public API: Get profiling data from the last profiled message */
AwsIotProfilingData_t aws_iot_get_profiling_data(void)
{
  return aws_iot_profiling_data;
}

/** @brief Public API: Reset profiling data */
void aws_iot_reset_profiling(void)
{
  memset(&aws_iot_profiling_data, 0, sizeof(aws_iot_profiling_data));
  ESP_LOGI(TAG_AWS_IOT, "Profiling data reset");
}

/** @brief Public API: Publish a log message to AWS IoT */
void aws_iot_publish_log(const char *message)
{
  if (!aws_iot_is_connected())
  {
    ESP_LOGW(TAG_AWS_IOT, "Cannot publish log message when MQTT is not connected");
    return;
  }

  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_PUBLISH_LOG;
  strncpy(msg.data.log_data.message, message, MAX_LOG_MESSAGE_LEN - 1);
  msg.data.log_data.message[MAX_LOG_MESSAGE_LEN - 1] = '\0';

  aws_iot_post_msg(msg);
}

/** @brief AWS IoT Keep Alive Task */
static void aws_iot_keep_alive_task()
{
  ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive Task started.");

  while (!keep_alive_should_stop)
  {
    xSemaphoreTake(mqtt_process_mutex, portMAX_DELAY);
    MQTTStatus_t mqtt_status = MQTT_ProcessLoop(&mqtt_context);
    xSemaphoreGive(mqtt_process_mutex);

    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes)
    {
      ESP_LOGE(TAG_AWS_IOT, "MQTT_ProcessLoop failed in Keep Alive task: %s",
               MQTT_Status_strerror(mqtt_status));

      state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);

      ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive Task exiting due to error.");
      keep_alive_task_handle = NULL;
      vTaskDelete(NULL);
    }

    uint32_t current_time_ms = Clock_GetTimeMs();
    ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive ping at %lu ms", current_time_ms);
    char message[128];
    sprintf(message, "AWS IoT Keep Alive ping at %lu ms", current_time_ms);
    aws_iot_publish_log(message);

    vTaskDelay(pdMS_TO_TICKS(MQTT_KEEP_ALIVE_TIMER_INTERVAL_MS));
  }

  ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive Task exiting gracefully.");
  keep_alive_task_handle = NULL;
  vTaskDelete(NULL);
}

/** @brief Disconnect the TLS connection
 *  @return ESP_OK if disconnected successfully, ESP_FAIL otherwise
 */
static int aws_iot_tls_disconnect()
{
  if (network_context.pxTls == NULL)
  {
    ESP_LOGW(TAG_AWS_IOT, "TLS is not connected, skipping disconnect.");
    return ESP_OK;
  }

  ESP_LOGI(TAG_AWS_IOT, "Disconnecting TLS...");

  TlsTransportStatus_t tls_status = xTlsDisconnect(&network_context);
  if (tls_status != TLS_TRANSPORT_SUCCESS)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to disconnect TLS cleanly: Status = %d", tls_status);
    return ESP_FAIL;
  }

  network_context.pxTls = NULL;

  ESP_LOGI(TAG_AWS_IOT, "TLS disconnected successfully.");
  return ESP_OK;
}

/** @brief Establish a TLS connection and MQTT session with AWS IoT broker
 *  @param broker_url The AWS IoT broker URL
 *  @param client_identifier The MQTT client identifier
 *  @return MQTTStatus_t indicating success or failure
 */
static MQTTStatus_t aws_iot_establish_mqtt_connection(const char *broker_url, const char *client_identifier)
{
  // Validate broker URL
  if (broker_url == NULL || strlen(broker_url) == 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "Invalid broker URL: %s", broker_url);
    return MQTTBadParameter;
  }

  MQTTStatus_t mqtt_status;
  ESP_LOGI(TAG_AWS_IOT, "Establishing TLS connection to %s...", broker_url);

  xSemaphoreTake(network_context.xTlsContextSemaphore, portMAX_DELAY);
  network_context.pcHostname = broker_url;
  connect_info.cleanSession = !session_present;
  connect_info.pClientIdentifier = client_identifier;
  connect_info.clientIdentifierLength = strlen(client_identifier);
  xSemaphoreGive(network_context.xTlsContextSemaphore);

  TlsTransportStatus_t tls_status = xTlsConnect(&network_context);
  if (tls_status != TLS_TRANSPORT_SUCCESS)
  {
    switch (tls_status)
    {
    case TLS_TRANSPORT_INVALID_CREDENTIALS:
      ESP_LOGE(TAG_AWS_IOT, "TLS connection failed: Invalid credentials - check certificate/key");
      break;
    case TLS_TRANSPORT_HANDSHAKE_FAILED:
      ESP_LOGE(TAG_AWS_IOT, "TLS connection failed: TLS handshake failed - check CA cert and endpoint URL");
      break;
    case TLS_TRANSPORT_CONNECT_FAILURE:
      ESP_LOGE(TAG_AWS_IOT, "TLS connection failed: TLS Connection failure - check network and endpoint URL");
      break;
    default:
      ESP_LOGE(TAG_AWS_IOT, "TLS connection failed: Unknown TLS error");
      break;
    }
    return MQTTServerRefused;
  }

  ESP_LOGI(TAG_AWS_IOT, "TLS connection established successfully.");
  ESP_LOGI(TAG_AWS_IOT, "Establishing MQTT connection...");

  // Verify MQTT client identifier is set
  if (connect_info.pClientIdentifier == NULL || connect_info.clientIdentifierLength == 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT client identifier is not set");
    // Disconnect TLS on MQTT connection failure
    if (aws_iot_tls_disconnect() != ESP_OK)
    {
      ESP_LOGE(TAG_AWS_IOT, "Failed to disconnect TLS after MQTT connection failure.");
    }
    return MQTTBadParameter;
  }

  // Now connect MQTT over the established TLS connection
  mqtt_status = MQTT_Connect(&mqtt_context, &connect_info, NULL, MQTT_CONNACK_RECV_TIMEOUT_MS, &session_present);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT connection failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    ESP_LOGE(TAG_AWS_IOT, "Disconnecting TLS due to MQTT connection failure.");
    // Disconnect TLS on MQTT connection failure
    if (aws_iot_tls_disconnect() != ESP_OK)
    {
      ESP_LOGE(TAG_AWS_IOT, "Failed to disconnect TLS after MQTT connection failure.");
    }

    return mqtt_status;
  }

  ESP_LOGI(TAG_AWS_IOT, "MQTT connection established successfully. Session Present: %s", session_present ? "true" : "false");
  return mqtt_status;
}

/** @brief Subscribe to the configured AWS IoT topic
 *  @param topic The MQTT topic to subscribe to
 */
static void aws_iot_subscribe_to_topic_cmd(const char* topic)
{
  MQTTStatus_t mqtt_status;
  ESP_LOGI(TAG_AWS_IOT, "Subscribing to topic %s...", topic);

  // Verify topic parameter
  if (topic == NULL || strlen(topic) == 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "Invalid topic: %s", topic);
    return;
  }

  // Free previous topic if it exists
  if (mqtt_topic != NULL)
  {
    free(mqtt_topic);
    mqtt_topic = NULL;
  }
  (void)memset((void *)subscription_list, 0x00, sizeof(subscription_list));

  // Subscribe to one topic
  subscription_list[0].qos = MQTTQoS1;
  subscription_list[0].pTopicFilter = topic;
  subscription_list[0].topicFilterLength = strlen(topic);

  // Generate a unique packet identifier for the SUBSCRIBE packet
  subscribe_packet_identifier = MQTT_GetPacketId(&mqtt_context);

  ESP_LOGI(TAG_AWS_IOT, "Sending SUBSCRIBE packet with ID %u", subscribe_packet_identifier);

  mqtt_status = MQTT_Subscribe(&mqtt_context,
                                subscription_list,
                                sizeof(subscription_list) / sizeof(MQTTSubscribeInfo_t),
                                subscribe_packet_identifier);

  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to send SUBSCRIBE packet: %s", MQTT_Status_strerror(mqtt_status));
    return;
  }

  // Wait for SUBACK and verify subscription
  const TickType_t start_time = xTaskGetTickCount();
  const TickType_t timeout_ticks = pdMS_TO_TICKS(MQTT_PROCESS_LOOP_TIMEOUT_MS);
  EventBits_t bits = 0;

  while ((xTaskGetTickCount() - start_time) < timeout_ticks)
  {
    // Check if event has already been set
    bits = xEventGroupGetBits(mqtt_event_group);
    if (bits & MQTT_SUBACK_RECEIVED_BIT)
    {
      break;
    }

    // Process MQTT events to receive SUBACK
    xSemaphoreTake(mqtt_process_mutex, portMAX_DELAY);
    mqtt_status = MQTT_ProcessLoop(&mqtt_context);
    xSemaphoreGive(mqtt_process_mutex);

    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes)
    {
      ESP_LOGE(TAG_AWS_IOT, "MQTT_ProcessLoop failed while waiting for SUBACK: %s",
               MQTT_Status_strerror(mqtt_status));
      return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  xEventGroupClearBits(mqtt_event_group, MQTT_SUBACK_RECEIVED_BIT | MQTT_SUBACK_SUCCESS_BIT);

  if (!(bits & MQTT_SUBACK_RECEIVED_BIT))
  {
    ESP_LOGE(TAG_AWS_IOT, "Timeout waiting for SUBACK");
    return;
  }

  if (!(bits & MQTT_SUBACK_SUCCESS_BIT))
  {
    ESP_LOGE(TAG_AWS_IOT, "Subscription rejected by broker");
    return;
  }

  // Allocate and copy the topic string
  mqtt_topic = malloc(strlen(topic) + 1);
  if (mqtt_topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to allocate memory for topic");
    return;
  }
  strcpy(mqtt_topic, topic);
  ESP_LOGI(TAG_AWS_IOT, "Subscribed to topic %s successfully.", mqtt_topic);

  state_machine_post_event(APP_AWS_IOT_EVT_SUBSCRIBED, APP_AWS_IOT);
}

/** @brief Handle the AWS IoT connect command */
static void aws_iot_connect_cmd(const char* broker_url, const char* client_identifier)
{
  ESP_LOGI(TAG_AWS_IOT, "%s connecting to AWS IoT broker at %s...", client_identifier, broker_url);

  // Free previous client identifier if it exists
  if (mqtt_client_identifier != NULL)
  {
    free(mqtt_client_identifier);
    mqtt_client_identifier = NULL;
  }

  // Free previous broker URL if it exists
  if (mqtt_broker_url != NULL)
  {
    free(mqtt_broker_url);
    mqtt_broker_url = NULL;
  }
  aws_iot_statistics.connection_attempts++;

  // Validate client identifier
  if (client_identifier == NULL || strlen(client_identifier) == 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "Invalid client identifier: %s", client_identifier);
    state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);
    return;
  }

  // Validate broker URL
  if (broker_url == NULL || strlen(broker_url) == 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "Invalid broker URL: %s", broker_url);
    state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);
    return;
  }

  MQTTStatus_t mqtt_status = aws_iot_establish_mqtt_connection(broker_url, client_identifier);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "%s failed to establish TLS/MQTT session to AWS IoT broker %s: %s", client_identifier, broker_url, MQTT_Status_strerror(mqtt_status));
    state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);
    return;
  }

  // Allocate and copy the client identifier string
  mqtt_client_identifier = malloc(strlen(client_identifier) + 1);
  if (mqtt_client_identifier == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to allocate memory for client identifier");
    return;
  }
  strcpy(mqtt_client_identifier, client_identifier);

  // Allocate and copy the broker URL string
  mqtt_broker_url = malloc(strlen(broker_url) + 1);
  if (mqtt_broker_url == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to allocate memory for broker URL");
    return;
  }
  strcpy(mqtt_broker_url, broker_url);
  
  state_machine_post_event(APP_AWS_IOT_EVT_CONNECTED, APP_AWS_IOT);
  ESP_LOGI(TAG_AWS_IOT, "%s connected to AWS IoT %s successfully.", mqtt_client_identifier, mqtt_broker_url);

  aws_iot_statistics.successful_connections++;
}

/** @brief Handle the AWS IoT start listening command */
static void aws_iot_start_listening_cmd()
{
  if (keep_alive_task_handle != NULL)
  {
    ESP_LOGW(TAG_AWS_IOT, "Keep Alive task already running.");
    return;
  }

  // Reset shutdown flag and start Keep Alive task
  keep_alive_should_stop = false;
  xTaskCreate(aws_iot_keep_alive_task, TAG_AWS_IOT_KEEP_ALIVE, MQTT_KEEP_ALIVE_TASK_STACK_SIZE, NULL, 5, &keep_alive_task_handle);
}

/** @brief Create a JSON payload for the button event
 *  @param state The button state string ("pressed" or "released")
 *  @return Pointer to the JSON payload string
 */
static const char *aws_iot_create_payload(const char *state, unsigned int duration_ms)
{
  // Prepare the MQTT PUBLISH message
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for button event");
    return NULL;
  }

  // Verify the client identifier is set
  if (mqtt_client_identifier == NULL || mqtt_client_identifier[0] == '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT client identifier is not set, cannot publish button event.");
    cJSON_Delete(json);
    return NULL;
  }

  // Add fields to JSON object
  if (cJSON_AddStringToObject(json, MSG_CLIENT_TAG, mqtt_client_identifier) == NULL ||
      cJSON_AddStringToObject(json, MSG_BUTTON_TAG, state) == NULL ||
      cJSON_AddNumberToObject(json, MSG_DURATION_MS_TAG, duration_ms) == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for button event");
    cJSON_Delete(json);
    return NULL;
  }

  const char *payload = cJSON_PrintUnformatted(json);
  if (payload == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to serialize JSON payload for button event");
    cJSON_Delete(json);
    return NULL;
  }

  cJSON_Delete(json);
  return payload;
}

/** @brief Create a JSON payload for the log message
 *  @param log_message The log message string
 *  @return Pointer to the JSON payload string
 */
static const char *aws_iot_create_payload_log(const char *log_message)
{
  // Prepare the MQTT PUBLISH message
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for log message");
    return NULL;
  }

  // Verify the client identifier is set
  if (mqtt_client_identifier == NULL || mqtt_client_identifier[0] == '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT client identifier is not set, cannot publish log message.");
    cJSON_Delete(json);
    return NULL;
  }

  // Add fields to JSON object
  if (cJSON_AddStringToObject(json, MSG_CLIENT_TAG, mqtt_client_identifier) == NULL ||
      cJSON_AddStringToObject(json, MSG_LOG_TAG, log_message) == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for log message");
    cJSON_Delete(json);
    return NULL;
  }

  const char *payload = cJSON_PrintUnformatted(json);
  if (payload == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to serialize JSON payload for log message");
    cJSON_Delete(json);
    return NULL;
  }

  cJSON_Delete(json);
  return payload;
}

/** @brief Handle the AWS IoT publish button event command
 *  @param state The button state string ("pressed" or "released")
 */
static void aws_iot_publish_button_event_cmd(const char* state, unsigned int duration_ms)
{
  // Check if MQTT is connected
  if (mqtt_context.connectStatus != MQTTConnected)
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot publish button event: MQTT not connected");
    return;
  }

  const char *payload = aws_iot_create_payload(state, duration_ms);
  if (payload == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create payload for button event");
    return;
  }

  MQTTPublishInfo_t publish_info = {0};

  const char *topic = aws_iot_get_mqtt_topic();
  if (topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to retrieve MQTT topic for publishing.");
    free((void *)payload);
    return;
  }

  // Get a unique packet identifier for the PUBLISH packet
  uint16_t packet_id = MQTT_GetPacketId(&mqtt_context);

  publish_info.qos = MQTTQoS1;
  publish_info.pTopicName = topic;
  publish_info.topicNameLength = strlen(topic);
  publish_info.pPayload = (const void *)payload;
  publish_info.payloadLength = strlen(payload);
  
  MQTTStatus_t mqtt_status = MQTT_Publish(&mqtt_context, &publish_info, packet_id);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to publish button event: %s", MQTT_Status_strerror(mqtt_status));
    free((void *)payload);
    return;
  }

  // Capture profiling timestamp if profiling is active
  if (aws_iot_profiling_data.profiling_active)
  {
    aws_iot_profiling_data.publish_timestamp_ms = Clock_GetTimeMs();
    aws_iot_profiling_data.publish_packet_id = packet_id;
    ESP_LOGI(TAG_AWS_IOT, "[PROFILING] Message published at %lu ms (Packet ID: %u)",
             aws_iot_profiling_data.publish_timestamp_ms, packet_id);
  }

  // Wait for PUBACK (QoS 1 requires acknowledgment)
  expected_puback_packet_id = packet_id;
  xEventGroupClearBits(mqtt_event_group, MQTT_PUBACK_RECEIVED_BIT);

  const TickType_t start_time = xTaskGetTickCount();
  const TickType_t timeout_ticks = pdMS_TO_TICKS(MQTT_PROCESS_LOOP_TIMEOUT_MS);
  EventBits_t bits = 0;

  while ((xTaskGetTickCount() - start_time) < timeout_ticks)
  {
    // Check if PUBACK has already been received
    bits = xEventGroupGetBits(mqtt_event_group);
    if (bits & MQTT_PUBACK_RECEIVED_BIT)
    {
      break;
    }

    // Process MQTT events to receive PUBACK
    xSemaphoreTake(mqtt_process_mutex, portMAX_DELAY);
    mqtt_status = MQTT_ProcessLoop(&mqtt_context);
    xSemaphoreGive(mqtt_process_mutex);
    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes)
    {
      ESP_LOGE(TAG_AWS_IOT, "MQTT_ProcessLoop failed while waiting for PUBACK: %s",
               MQTT_Status_strerror(mqtt_status));
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to avoid tight loop
  }

  xEventGroupClearBits(mqtt_event_group, MQTT_PUBACK_RECEIVED_BIT);
  expected_puback_packet_id = 0;

  if (!(bits & MQTT_PUBACK_RECEIVED_BIT))
  {
    ESP_LOGW(TAG_AWS_IOT, "Timeout waiting for PUBACK (Packet ID: %u)", packet_id);
  }

  ESP_LOGI(TAG_AWS_IOT, "Published button event to topic %s: %s", topic, payload);
  char message[128];
  sprintf(message, "Published button event to topic %s at %lu ms: %s", topic, aws_iot_profiling_data.publish_timestamp_ms, payload);
  aws_iot_publish_log(message);

  aws_iot_statistics.messages_published++;

  free((void *)payload);
}

static void aws_iot_publish_log_cmd(const char* log_message)
{
  // TODO - Implement log message publishing to AWS IoT similar to button event publishing
  ESP_LOGI(TAG_AWS_IOT, "Log message to publish: %s", log_message);

  // Check if MQTT is connected
  if (!aws_iot_is_connected())
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot publish log message: MQTT not connected");
    return;
  }

  const char *payload = aws_iot_create_payload_log(log_message);
  if (payload == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create payload for log message");
    return;
  }

  MQTTPublishInfo_t publish_info = {0};

  const char *topic = aws_iot_get_mqtt_log_topic();
  if (topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to retrieve MQTT topic for publishing.");
    free((void *)payload);
    return;
  }

  // Get a unique packet identifier for the PUBLISH packet
  uint16_t packet_id = MQTT_GetPacketId(&mqtt_context);

  publish_info.qos = MQTTQoS1;
  publish_info.pTopicName = topic;
  publish_info.topicNameLength = strlen(topic);
  publish_info.pPayload = (const void *)payload;
  publish_info.payloadLength = strlen(payload);

  MQTTStatus_t mqtt_status = MQTT_Publish(&mqtt_context, &publish_info, packet_id);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to publish log message: %s", MQTT_Status_strerror(mqtt_status));
    free((void *)payload);
    return;
  }
}

/** @brief Handle incoming PUBLISH packet
 *  @param p_packet_info Pointer to the MQTT packet info
 */
static void aws_iot_handle_publish_packet(MQTTPublishInfo_t * p_publish_info, unsigned int packet_id)
{
  ESP_LOGI(TAG_AWS_IOT, "Handling incoming PUBLISH packet ID %u on topic %.*s", 
    packet_id,
    p_publish_info->topicNameLength,
    p_publish_info->pTopicName);

  // Capture profiling timestamp if profiling is active
  if (aws_iot_profiling_data.profiling_active && aws_iot_profiling_data.receive_timestamp_ms == 0)
  {
    aws_iot_profiling_data.receive_timestamp_ms = Clock_GetTimeMs();
    ESP_LOGI(TAG_AWS_IOT, "[PROFILING] Message received at %lu ms (Packet ID: %u)",
             aws_iot_profiling_data.receive_timestamp_ms, packet_id);
  }

  // Parse JSON payload
  cJSON *json = cJSON_ParseWithLength(p_publish_info->pPayload, p_publish_info->payloadLength);
  if (json == NULL)
  {
    const char *error_ptr = cJSON_GetErrorPtr();
    ESP_LOGE(TAG_AWS_IOT, "Failed to parse JSON payload before: %s", error_ptr ? error_ptr : "unknown error");
    return;
  }

  cJSON *client_json = cJSON_GetObjectItemCaseSensitive(json, MSG_CLIENT_TAG);
  cJSON *duration_json = cJSON_GetObjectItemCaseSensitive(json, MSG_DURATION_MS_TAG);

  if (cJSON_IsString(client_json) && (client_json->valuestring != NULL) &&
      cJSON_IsNumber(duration_json) && (duration_json->valueint >= 0))
  {
    ESP_LOGI(TAG_AWS_IOT, "Received button event from client: %s, duration: %d ms",
             client_json->valuestring,
             duration_json->valueint);

    aws_iot_statistics.messages_received++;

    // Verify client identifier is set
    if (mqtt_client_identifier == NULL || mqtt_client_identifier[0] == '\0')
    {
      ESP_LOGW(TAG_AWS_IOT, "MQTT client identifier is not set, ignoring message.");
      cJSON_Delete(json);
      return;
    }

    // If client identifier does not match this device, notify the State Machine task
    if (strcmp(client_json->valuestring, mqtt_client_identifier) != 0)
    {
      ESP_LOGI(TAG_AWS_IOT, "Handling button event from %s", client_json->valuestring);

      AwsIotMsg_t aws_iot_msg = {
        .type = APP_AWS_IOT_EVT_MSG_PRESSED,
        .data = {
          .button_event = {
            .duration_ms = (unsigned int)duration_json->valueint
          }
        }
      };

      AppMsg_t app_msg = {
        .type = APP_AWS_IOT_EVT_MSG_PRESSED,
        .data = {.aws_iot = aws_iot_msg},
        .source = APP_AWS_IOT
      };

      state_machine_post_event_msg(&app_msg);
    }
  }
  else
  {
    ESP_LOGW(TAG_AWS_IOT, "Invalid JSON payload structure");
  }

  cJSON_Delete(json);
}

/** @brief Handle incoming SUBACK packet
 *  @param p_packet_info Pointer to the MQTT packet info
 */
static void aws_iot_handle_suback_packet(MQTTPacketInfo_t * p_packet_info)
{
  uint8_t *payload = NULL;
  size_t payload_size = 0;

  MQTTStatus_t mqtt_status = MQTT_GetSubAckStatusCodes(p_packet_info, &payload, &payload_size);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to get SUBACK status codes: %s", MQTT_Status_strerror(mqtt_status));
    return;
  }

  // Publish SUBACK status
  ESP_LOGI(TAG_AWS_IOT, "SUBACK Status Code: %u", payload[0]);

  const char *topic = subscription_list[0].pTopicFilter;
  if (topic == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to retrieve subscribed topic for SUBACK handling.");
    return;
  }

  if (payload[0] != MQTTSubAckFailure)
  {
    ESP_LOGI(TAG_AWS_IOT, "Subscription to topic %s successful.", topic);
    xEventGroupSetBits(mqtt_event_group, MQTT_SUBACK_RECEIVED_BIT | MQTT_SUBACK_SUCCESS_BIT);
  }
  else
  {
    ESP_LOGE(TAG_AWS_IOT, "Subscription failed with status code: %u", payload[0]);
    xEventGroupSetBits(mqtt_event_group, MQTT_SUBACK_RECEIVED_BIT);
  }
}

/** @brief AWS IoT MQTT Event Callback
 *  @param p_mqtt_context Pointer to the MQTT context
 *  @param p_packet_info Pointer to the MQTT packet info
 *  @param p_deserialized_info Pointer to the MQTT deserialized info
 */
static void aws_iot_event_callback(MQTTContext_t * p_mqtt_context,
                                   MQTTPacketInfo_t * p_packet_info,
                                   MQTTDeserializedInfo_t * p_deserialized_info)
{
  uint16_t packet_id = p_deserialized_info->packetIdentifier;
  ESP_LOGI(TAG_AWS_IOT, "AWS IoT MQTT Event Callback: Packet Type=%02X, Packet ID=%u",
           p_packet_info->type,
           packet_id);

  if ((p_packet_info->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
  {
    ESP_LOGI(TAG_AWS_IOT, "Received MQTT PUBLISH packet (Packet ID: %u)", packet_id);
    aws_iot_handle_publish_packet(p_deserialized_info->pPublishInfo, packet_id);
  }
  else
  {
    switch (p_packet_info->type)
    {
      case MQTT_PACKET_TYPE_SUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received MQTT SUBACK packet (Packet ID: %u)", packet_id);
        aws_iot_handle_suback_packet(p_packet_info);
        break;
      case MQTT_PACKET_TYPE_UNSUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received MQTT UNSUBACK packet (Packet ID: %u)", packet_id);
        // TODO - Handle UNSUBACK
        break;

      case MQTT_PACKET_TYPE_PUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received MQTT PUBACK packet (Packet ID: %u)", packet_id);
        // Capture profiling timestamp if profiling is active and packet ID matches
        if (aws_iot_profiling_data.profiling_active && 
            aws_iot_profiling_data.publish_packet_id == packet_id)
        {
          aws_iot_profiling_data.puback_timestamp_ms = Clock_GetTimeMs();
          ESP_LOGI(TAG_AWS_IOT, "[PROFILING] PUBACK received at %lu ms (Packet ID: %u)",
                   aws_iot_profiling_data.puback_timestamp_ms, packet_id);
        }
        // Signal PUBACK received if we're waiting for this packet ID
        if (expected_puback_packet_id == packet_id)
        {
          xEventGroupSetBits(mqtt_event_group, MQTT_PUBACK_RECEIVED_BIT);
        }
        break;

      case MQTT_PACKET_TYPE_PINGRESP:
        ESP_LOGI(TAG_AWS_IOT, "Received MQTT PINGRESP packet (Packet ID: %u)", packet_id);
        // TODO - Handle PINGRESP
        break;

      case MQTT_PACKET_TYPE_CONNACK:
        ESP_LOGI(TAG_AWS_IOT, "Received MQTT CONNACK packet (Packet ID: %u)", packet_id);
        // TODO - Handle CONNACK
        break;

      default:
        ESP_LOGW(TAG_AWS_IOT, "Unhandled MQTT packet type: %02X", p_packet_info->type);
        break;
    }
  }
}

/** @brief Initialize AWS IoT Task
 *  @param self Pointer to the generic task object for the AWS IoT task
 */
static esp_err_t aws_iot_on_init(GenericTask *self)
{
  MQTTStatus_t mqtt_status = MQTTSuccess;
  MQTTFixedBuffer_t network_buffer;
  TransportInterface_t transport = {0};
  esp_err_t ret;

  // Step 0: Clear network context (important for re-initialization between tests)
  memset(&network_context, 0, sizeof(NetworkContext_t));

  // Step 1: Create MQTT event group
  mqtt_event_group = xEventGroupCreate();
  if (mqtt_event_group == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create MQTT event group");
    return ESP_ERR_NO_MEM;
  }

  // Step 2: Create MQTT process mutex
  mqtt_process_mutex = xSemaphoreCreateMutex();
  if (mqtt_process_mutex == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create MQTT process mutex");
    vEventGroupDelete(mqtt_event_group);
    mqtt_event_group = NULL;
    return ESP_ERR_NO_MEM;
  }

  // Step 2: Initialize transport interface
  transport.pNetworkContext = &network_context;
  transport.send = espTlsTransportSend;
  transport.recv = espTlsTransportRecv;
  transport.writev = NULL;

  network_buffer.pBuffer = buffer;
  network_buffer.size = MQTT_NETWORK_BUFFER_SIZE;

  // Step 3: Initialize MQTT context
  mqtt_status = MQTT_Init(&mqtt_context,
                          &transport,
                          Clock_GetTimeMs,
                          aws_iot_event_callback,
                          &network_buffer);

  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT_Init failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    ret = ESP_FAIL;
    goto cleanup_event_group;
  }

  // Step 4: Initialize stateful QoS
  mqtt_status = MQTT_InitStatefulQoS(&mqtt_context,
                                     outgoing_publish_records,
                                     MQTT_OUTGOING_PUBLISH_RECORD_LEN,
                                     incoming_publish_records,
                                     MQTT_INCOMING_PUBLISH_RECORD_LEN);

  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT_InitStatefulQoS failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    ret = ESP_FAIL;
    goto cleanup_event_group;
  }

  ESP_LOGI(TAG_AWS_IOT, "MQTT initialized successfully.");

  // Step 5: Initialize network context
  network_context.xPort = MQTT_PORT;
  network_context.pxTls = NULL;
  network_context.xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&tls_context_semaphore);
  network_context.disableSni = 0;

  if (network_context.xTlsContextSemaphore == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create TLS context semaphore.");
    ret = ESP_ERR_NO_MEM;
    goto cleanup_event_group;
  }
  
  // Step 6: Initialize TLS connection credentials
  network_context.pcServerRootCA = root_cert_auth_start;
  network_context.pcServerRootCASize = root_cert_auth_end - root_cert_auth_start;
  network_context.pcClientCert = client_cert_start;
  network_context.pcClientCertSize = client_cert_end - client_cert_start;
  network_context.pcClientKey = client_key_start;
  network_context.pcClientKeySize = client_key_end - client_key_start;

  if (network_context.pcServerRootCA == NULL || network_context.pcServerRootCASize <= 0 ||
      network_context.pcClientCert == NULL || network_context.pcClientCertSize <= 0 ||
      network_context.pcClientKey == NULL || network_context.pcClientKeySize <= 0)
  {
    ESP_LOGE(TAG_AWS_IOT, "TLS credentials are not properly configured.");
    ret = ESP_FAIL;
    goto cleanup_semaphore;
  }

  // Verify certificates are null-terminated (required for mbedTLS PEM parsing)
  if (root_cert_auth_start[network_context.pcServerRootCASize - 1] != '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "Root CA certificate is not null-terminated!");
    ret = ESP_FAIL;
    goto cleanup_semaphore;
  }
  if (client_cert_start[network_context.pcClientCertSize - 1] != '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "Client certificate is not null-terminated!");
    ret = ESP_FAIL;
    goto cleanup_semaphore;
  }
  if (client_key_start[network_context.pcClientKeySize - 1] != '\0')
  {
    ESP_LOGE(TAG_AWS_IOT, "Client private key is not null-terminated!");
    ret = ESP_FAIL;
    goto cleanup_semaphore;
  }

  ESP_LOGI(TAG_AWS_IOT, "AWS IoT TLS credentials");
  ESP_LOGI(TAG_AWS_IOT, "Using AmazonRootCA1.pem (%d bytes)",
           (int)network_context.pcServerRootCASize);
  ESP_LOGI(TAG_AWS_IOT, "Using device_certificate.pem.crt (%d bytes)",
           (int)network_context.pcClientCertSize);
  ESP_LOGI(TAG_AWS_IOT, "Using private_key.pem.key (%d bytes)",
           (int)network_context.pcClientKeySize);

  network_context.pAlpnProtos = NULL;

  // Step 7: Initialize MQTT connection info
  connect_info.cleanSession = true;
  connect_info.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_S;

  ESP_LOGI(TAG_AWS_IOT, "AWS IoT Task initialized successfully.");
  return ESP_OK;

cleanup_semaphore:
  if (network_context.xTlsContextSemaphore != NULL)
  {
    vSemaphoreDelete(network_context.xTlsContextSemaphore);
    network_context.xTlsContextSemaphore = NULL;
  }
cleanup_event_group:
  vEventGroupDelete(mqtt_event_group);
  mqtt_event_group = NULL;
  return ret;
}

static esp_err_t aws_iot_on_stop(GenericTask *self)
{
  ESP_LOGI(TAG_AWS_IOT, "Stopping AWS IoT Task...");
  esp_err_t final_ret = ESP_OK;

  // Step 1: Signal Keep Alive task to stop gracefully
  if (keep_alive_task_handle != NULL)
  {
    ESP_LOGI(TAG_AWS_IOT, "Signaling Keep Alive task to stop...");
    keep_alive_should_stop = true;
    
    // Wait for the task to exit (with timeout)
    const TickType_t timeout = pdMS_TO_TICKS(2000);  // 2 second timeout
    const TickType_t start_time = xTaskGetTickCount();
    
    while (keep_alive_task_handle != NULL && 
           (xTaskGetTickCount() - start_time) < timeout)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // If task didn't exit gracefully, force delete it
    if (keep_alive_task_handle != NULL)
    {
      ESP_LOGW(TAG_AWS_IOT, "Keep Alive task didn't exit gracefully, force deleting...");
      vTaskDelete(keep_alive_task_handle);
      keep_alive_task_handle = NULL;
    }
    else
    {
      ESP_LOGI(TAG_AWS_IOT, "Keep Alive task stopped gracefully.");
    }
  }

  // Step 2: Disconnect MQTT if connected
  if (mqtt_context.connectStatus == MQTTConnected)
  {
    ESP_LOGI(TAG_AWS_IOT, "Disconnecting MQTT...");
    MQTTStatus_t mqtt_status = MQTT_Disconnect(&mqtt_context);
    if (mqtt_status != MQTTSuccess)
    {
      ESP_LOGE(TAG_AWS_IOT, "Failed to disconnect MQTT cleanly: %s", MQTT_Status_strerror(mqtt_status));
      final_ret = ESP_FAIL;
      // Continue cleanup anyway
    }
    else
    {
      ESP_LOGI(TAG_AWS_IOT, "MQTT disconnected successfully."); 
    }
    
    // Reset MQTT context state after disconnect
    mqtt_context.connectStatus = MQTTNotConnected;
  }
  else
  {
    ESP_LOGI(TAG_AWS_IOT, "MQTT not connected, skipping disconnect.");
  }

  // Step 3: Disconnect TLS if connected
  // Only call xTlsDisconnect if pxTls is valid
  if (aws_iot_tls_disconnect() != ESP_OK)
  {
    final_ret = ESP_FAIL;
    // Continue cleanup anyway
  }

  // Step 4: Delete TLS context semaphore
  if (network_context.xTlsContextSemaphore != NULL)
  {
    vSemaphoreDelete(network_context.xTlsContextSemaphore);
    network_context.xTlsContextSemaphore = NULL;

    ESP_LOGI(TAG_AWS_IOT, "TLS context semaphore deleted.");
  }

  // Step 5: Delete event group
  if (mqtt_event_group != NULL)
  {
    vEventGroupDelete(mqtt_event_group);
    mqtt_event_group = NULL;

    ESP_LOGI(TAG_AWS_IOT, "MQTT event group deleted.");
  }

  // Step 6: Delete MQTT process mutex
  if (mqtt_process_mutex != NULL)
  {
    vSemaphoreDelete(mqtt_process_mutex);
    mqtt_process_mutex = NULL;

    ESP_LOGI(TAG_AWS_IOT, "MQTT process mutex deleted.");
  }

  // Step 7: Free allocated strings
  if (mqtt_broker_url != NULL)
  {
    free(mqtt_broker_url);
    mqtt_broker_url = NULL;
  }

  if (mqtt_topic != NULL)
  {
    free(mqtt_topic);
    mqtt_topic = NULL;
  }

  // Step 8: Reset shutdown flag
  keep_alive_should_stop = false;

  // Step 9: Clear statistics
  (void)memset(&aws_iot_statistics, 0, sizeof(aws_iot_statistics));

  // Step 10: Clear profiling data
  (void)memset(&aws_iot_profiling_data, 0, sizeof(aws_iot_profiling_data));

  ESP_LOGI(TAG_AWS_IOT, "AWS IoT Task stopped.");
  return final_ret;
}

/** @brief Handle messages for AWS IoT Task
 *  @param self Pointer to the generic task object for the AWS IoT task
 *  @param msg_buf The message buffer queue for this task
 *  @param msg_len The length of a message for the message buffer
 */
static void aws_iot_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  if (msg_len != sizeof(AwsIotMsg_t))
  {
    return;
  }

  AwsIotMsg_t *msg = (AwsIotMsg_t *)msg_buf;

  // Handle AWS IoT messages based on their type
  switch (msg->type)
  {
    case APP_AWS_IOT_CMD_CONNECT:
      aws_iot_connect_cmd(msg->data.connect_data.broker_url, msg->data.connect_data.client_identifier);
      break;
    case APP_AWS_IOT_CMD_SUBSCRIBE:
      aws_iot_subscribe_to_topic_cmd(msg->data.topic);
      break;
    case APP_AWS_IOT_CMD_START_LISTENING:
      aws_iot_start_listening_cmd();
      break;
    case APP_AWS_IOT_CMD_PUBLISH_BUTTON_EVENT:
      aws_iot_publish_button_event_cmd(msg->data.button_event.button_state, msg->data.button_event.duration_ms);
      break;
    case APP_AWS_IOT_CMD_PUBLISH_LOG:
      aws_iot_publish_log_cmd(msg->data.log_data.message);
      break;
    default:
      break;
  }
}

/** @brief Create AWS IoT Task */
esp_err_t aws_iot_task_init(void)
{
  aws_iot_task = generic_task_create(
      TAG_AWS_IOT,
      sizeof(AwsIotMsg_t),
      aws_iot_on_init,
      aws_iot_on_message,
      aws_iot_on_stop);

  if (aws_iot_task == NULL)
  {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t ret = generic_task_start(aws_iot_task, MQTT_AWS_IOT_TASK_STACK_SIZE, 10);
  if (ret != ESP_OK)
  {
    generic_task_delete(aws_iot_task);
    aws_iot_task = NULL;
    return ret;
  }

  return ESP_OK;
}

esp_err_t aws_iot_task_deinit(void)
{
  if (aws_iot_task == NULL)
  {
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret = generic_task_stop(aws_iot_task);
  if (ret != ESP_OK)
  {
    return ret;
  }

  ret = generic_task_delete(aws_iot_task);
  if (ret != ESP_OK)
  {
    return ret;
  }

  aws_iot_task = NULL;
  return ESP_OK;
}