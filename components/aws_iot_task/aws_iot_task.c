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

// User definitions
#ifndef MQTT_BROKER_ENDPOINT
#error "MQTT_BROKER_ENDPOINT must be defined in sdkconfig"
#endif

#ifndef MQTT_CLIENT_IDENTIFIER
#define MQTT_CLIENT_IDENTIFIER "Default"
#endif // MQTT_CLIENT_IDENTIFIER
#define MQTT_CLIENT_IDENTIFIER_LENGTH ((uint16_t)(sizeof(MQTT_CLIENT_IDENTIFIER) - 1))

#ifndef MQTT_TOPIC
#define MQTT_TOPIC "Default"
#endif // MQTT_TOPIC
#define MQTT_TOPIC_LENGTH ((uint16_t)(sizeof(MQTT_TOPIC) - 1))

#define MQTT_PORT 8883

// Time definitions
#define MQTT_KEEP_ALIVE_INTERVAL_S 60U
#define MQTT_KEEP_ALIVE_TIMER_INTERVAL_MS 500U
#define MQTT_CONNACK_RECV_TIMEOUT_MS 5000U
#define MQTT_PROCESS_LOOP_TIMEOUT_MS 5000U

#define MQTT_OUTGOING_PUBLISH_RECORD_LEN 20U
#define MQTT_INCOMING_PUBLISH_RECORD_LEN 20U

#define MQTT_NETWORK_BUFFER_SIZE 2048

#define MQTT_SUBACK_RECEIVED_BIT (1 << 0)
#define MQTT_SUBACK_SUCCESS_BIT (1 << 1)

extern const char root_cert_auth_start[] asm("_binary_AmazonRootCA1_pem_start");
extern const char root_cert_auth_end[] asm("_binary_AmazonRootCA1_pem_end");

extern const char client_cert_start[] asm("_binary_device_certificate_pem_crt_start");
extern const char client_cert_end[] asm("_binary_device_certificate_pem_crt_end");

extern const char client_key_start[] asm("_binary_private_key_pem_key_start");
extern const char client_key_end[] asm("_binary_private_key_pem_key_end");

// --- Module State ---
static const char *TAG_AWS_IOT = "AWS_IOT_TASK";
static const char *TAG_AWS_IOT_KEEP_ALIVE = "AWS_IOT_KEEP_ALIVE_TASK";
static GenericTask aws_iot_task;

static const char *MSG_CLIENT_TAG = "client";
static const char *MSG_BUTTON_TAG = "button";
static const char *MSG_BUTTON_PRESSED = "pressed";
static const char *MSG_BUTTON_RELEASED = "released";

// MQTT connection
static MQTTContext_t mqtt_context = {0};
static MQTTConnectInfo_t connect_info = {0};

// Network
static NetworkContext_t network_context = {0};
static uint8_t buffer[MQTT_NETWORK_BUFFER_SIZE];
static StaticSemaphore_t tls_context_semaphore;

// MQTT subscriptions
static MQTTSubscribeInfo_t subscription_list[1];
static unsigned short subscribe_packet_identifier = 0U;

// MQTT message queues
static MQTTPubAckInfo_t outgoing_publish_records[MQTT_OUTGOING_PUBLISH_RECORD_LEN];
static MQTTPubAckInfo_t incoming_publish_records[MQTT_INCOMING_PUBLISH_RECORD_LEN];

// Events handling
static bool session_present = false;
static EventGroupHandle_t mqtt_event_group = NULL;

static TaskHandle_t keep_alive_task_handle = NULL;

/** @brief Post a command message to the AWS IoT task
 *  @param msg The message to post
 */
static BaseType_t aws_iot_post_msg(AwsIotMsg_t msg)
{
  return generic_task_post_msg(&aws_iot_task, &msg, sizeof(AwsIotMsg_t));
}

/** @brief Public API: Request a connection to the configured AWS IoT broker */
void aws_iot_connect(void)
{
  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_CONNECT;
  aws_iot_post_msg(msg);
}

/** @brief Public API: Start listening for incoming MQTT messages from AWS IoT */
void aws_iot_start_listening(void)
{
  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_START_LISTENING;
  aws_iot_post_msg(msg);
}

/** @brief Public API: Publish a button press event to AWS IoT */
void aws_iot_publish_button_pressed_event(void)
{
  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_PUBLISH_BUTTON_PRESSED;
  aws_iot_post_msg(msg);
}

/** @brief Public API: Publish a button released event to AWS IoT */
void aws_iot_publish_button_released_event(void)
{
  AwsIotMsg_t msg;
  msg.type = APP_AWS_IOT_CMD_PUBLISH_BUTTON_RELEASED;
  aws_iot_post_msg(msg);
}

/** @brief AWS IoT Keep Alive Task */
static void aws_iot_keep_alive_task()
{
  ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive Task started.");

  while (true)
  {
    MQTTStatus_t mqtt_status = MQTT_ProcessLoop(&mqtt_context);

    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes)
    {
      ESP_LOGE(TAG_AWS_IOT, "MQTT_ProcessLoop failed in Keep Alive task: %s",
               MQTT_Status_strerror(mqtt_status));

      // Handle disconnection
      MQTT_Disconnect(&mqtt_context);
      xTlsDisconnect(&network_context);

      state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);

      ESP_LOGI(TAG_AWS_IOT_KEEP_ALIVE, "AWS IoT Keep Alive Task exiting.");
      keep_alive_task_handle = NULL;
      vTaskDelete(NULL);
    }

    vTaskDelay(pdMS_TO_TICKS(MQTT_KEEP_ALIVE_TIMER_INTERVAL_MS));
  }
}

/** @brief Establish a TLS connection and MQTT session with AWS IoT broker */
static MQTTStatus_t aws_iot_establish_mqtt_connection(void)
{
  MQTTStatus_t mqtt_status;
  ESP_LOGI(TAG_AWS_IOT, "Establishing TLS connection to %s...", MQTT_BROKER_ENDPOINT);

  xSemaphoreTake(network_context.xTlsContextSemaphore, portMAX_DELAY);
  connect_info.cleanSession = !session_present;
  xSemaphoreGive(network_context.xTlsContextSemaphore);

  TlsTransportStatus_t tls_status = xTlsConnect(&network_context);

  if (tls_status != TLS_TRANSPORT_SUCCESS)
  {
    ESP_LOGE(TAG_AWS_IOT, "TLS connection failed: Status = %d.", tls_status);

    switch (tls_status)
    {
    case TLS_TRANSPORT_INVALID_CREDENTIALS:
      ESP_LOGE(TAG_AWS_IOT, "Invalid credentials - check certificate/key");
      break;
    case TLS_TRANSPORT_HANDSHAKE_FAILED:
      ESP_LOGE(TAG_AWS_IOT, "TLS handshake failed - check CA cert and endpoint");
      break;
    case TLS_TRANSPORT_CONNECT_FAILURE:
      ESP_LOGE(TAG_AWS_IOT, "Connection failure - check network and endpoint");
      break;
    default:
      ESP_LOGE(TAG_AWS_IOT, "Unknown TLS error");
      break;
    }
    return MQTTServerRefused;
  }

  ESP_LOGI(TAG_AWS_IOT, "TLS connection established successfully.");
  ESP_LOGI(TAG_AWS_IOT, "Establishing MQTT connection...");

  // Now connect MQTT over the established TLS connection
  mqtt_status = MQTT_Connect(&mqtt_context, &connect_info, NULL, MQTT_CONNACK_RECV_TIMEOUT_MS, &session_present);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT connection failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    xTlsDisconnect(&network_context);
    return mqtt_status;
  }

  ESP_LOGI(TAG_AWS_IOT, "MQTT connection established successfully. Session Present: %s", session_present ? "true" : "false");
  return mqtt_status;
}

/** @brief Subscribe to the configured AWS IoT topic */
static MQTTStatus_t aws_iot_subscribe_to_topic()
{
  MQTTStatus_t mqtt_status;
  ESP_LOGI(TAG_AWS_IOT, "Subscribing to topic %s...", MQTT_TOPIC);

  (void)memset((void *)subscription_list, 0x00, sizeof(subscription_list));

  // Subscribe to one topic
  subscription_list[0].qos = MQTTQoS1;
  subscription_list[0].pTopicFilter = MQTT_TOPIC;
  subscription_list[0].topicFilterLength = MQTT_TOPIC_LENGTH;

  // Generate a unique packet identifier for the SUBSCRIBE packet
  subscribe_packet_identifier = MQTT_GetPacketId(&mqtt_context);

  ESP_LOGI(TAG_AWS_IOT, "Sending SUBSCRIBE packet with ID %u", subscribe_packet_identifier);

  mqtt_status = MQTT_Subscribe(&mqtt_context,
                                subscription_list,
                                sizeof(subscription_list) / sizeof(MQTTSubscribeInfo_t),
                                subscribe_packet_identifier);

  if (mqtt_status != MQTTSuccess)
  {
    return mqtt_status;
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
    mqtt_status = MQTT_ProcessLoop(&mqtt_context);

    if (mqtt_status != MQTTSuccess && mqtt_status != MQTTNeedMoreBytes)
    {
      ESP_LOGE(TAG_AWS_IOT, "MQTT_ProcessLoop failed while waiting for SUBACK: %s",
               MQTT_Status_strerror(mqtt_status));
      return mqtt_status;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  xEventGroupClearBits(mqtt_event_group, MQTT_SUBACK_RECEIVED_BIT | MQTT_SUBACK_SUCCESS_BIT);

  if (!(bits & MQTT_SUBACK_RECEIVED_BIT))
  {
    ESP_LOGE(TAG_AWS_IOT, "Timeout waiting for SUBACK");
    return MQTTRecvFailed;
  }

  if (!(bits & MQTT_SUBACK_SUCCESS_BIT))
  {
    ESP_LOGE(TAG_AWS_IOT, "Subscription rejected by broker");
    return MQTTServerRefused;
  }

  ESP_LOGI(TAG_AWS_IOT, "Subscribed to topic %s successfully.", MQTT_TOPIC);
  return mqtt_status;
}

/** @brief Handle the AWS IoT connect command */
static void aws_iot_connect_cmd(void)
{
  MQTTStatus_t mqtt_status = aws_iot_establish_mqtt_connection();
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to establish TLS/MQTT session to AWS IoT broker %s: %s", MQTT_BROKER_ENDPOINT, MQTT_Status_strerror(mqtt_status));
    state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);
    return;
  }

  mqtt_status = aws_iot_subscribe_to_topic();
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to subscribe to AWS IoT topic %s: %s", MQTT_TOPIC, MQTT_Status_strerror(mqtt_status));

    MQTT_Disconnect(&mqtt_context);
    xTlsDisconnect(&network_context);

    state_machine_post_event(APP_AWS_IOT_EVT_DISCONNECTED, APP_AWS_IOT);
    return;
  }

  state_machine_post_event(APP_AWS_IOT_EVT_CONNECTED, APP_AWS_IOT);
  ESP_LOGI(TAG_AWS_IOT, "Connected to AWS IoT %s successfully.", MQTT_BROKER_ENDPOINT);
}

/** @brief Handle the AWS IoT start listening command */
static void aws_iot_start_listening_cmd()
{
  if (keep_alive_task_handle != NULL)
  {
    ESP_LOGW(TAG_AWS_IOT, "Keep Alive task already running.");
    return;
  }

  // Start Keep Alive task
  xTaskCreate(aws_iot_keep_alive_task, TAG_AWS_IOT_KEEP_ALIVE, MQTT_KEEP_ALIVE_TASK_STACK_SIZE, NULL, 5, &keep_alive_task_handle);
}

/** @brief Handle the AWS IoT publish button event command */
static void aws_iot_publish_button_event_cmd(const char* state)
{
  // Check if MQTT is connected
  if (mqtt_context.connectStatus != MQTTConnected)
  {
    ESP_LOGE(TAG_AWS_IOT, "Cannot publish button event: MQTT not connected");
    return;
  }

  // Prepare the MQTT PUBLISH message
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for button event");
    return;
  }

  if (cJSON_AddStringToObject(json, MSG_CLIENT_TAG, MQTT_CLIENT_IDENTIFIER) == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for button event");
    cJSON_Delete(json);
    return;
  }

  if (cJSON_AddStringToObject(json, MSG_BUTTON_TAG, state) == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create JSON payload for button event");
    cJSON_Delete(json);
    return;
  }

  const char *payload = cJSON_PrintUnformatted(json);
  if (payload == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to serialize JSON payload for button event");
    cJSON_Delete(json);
    return;
  }

  cJSON_Delete(json);

  MQTTPublishInfo_t publish_info = {0};

  // Get a unique packet identifier for the PUBLISH packet
  uint16_t packet_id = MQTT_GetPacketId(&mqtt_context);

  publish_info.qos = MQTTQoS1;
  publish_info.pTopicName = MQTT_TOPIC;
  publish_info.topicNameLength = MQTT_TOPIC_LENGTH;
  publish_info.pPayload = (const void *)payload;
  publish_info.payloadLength = strlen(payload);
  
  MQTTStatus_t mqtt_status = MQTT_Publish(&mqtt_context, &publish_info, packet_id);
  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to publish button event: %s", MQTT_Status_strerror(mqtt_status));
    return;
  }

  // TODO - Wait for PUBACK

  ESP_LOGI(TAG_AWS_IOT, "Published button event to topic %s: %s", MQTT_TOPIC, payload);

  free((void *)payload);
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

  // TODO - Acknowledge the PUBLISH if QoS 1

  // Parse JSON payload
  cJSON *json = cJSON_ParseWithLength(p_publish_info->pPayload, p_publish_info->payloadLength);
  if (json == NULL)
  {
    const char *error_ptr = cJSON_GetErrorPtr();
    ESP_LOGE(TAG_AWS_IOT, "Failed to parse JSON payload before: %s", error_ptr ? error_ptr : "unknown error");
    return;
  }

  cJSON *client_json = cJSON_GetObjectItemCaseSensitive(json, MSG_CLIENT_TAG);
  cJSON *button_json = cJSON_GetObjectItemCaseSensitive(json, MSG_BUTTON_TAG);

  if (cJSON_IsString(client_json) && (client_json->valuestring != NULL) &&
      cJSON_IsString(button_json) && (button_json->valuestring != NULL))
  {
    ESP_LOGI(TAG_AWS_IOT, "Received button event from client: %s, button: %s",
             client_json->valuestring,
             button_json->valuestring);

    // If client identifier does not match this device, notify the State Machine task
    if (strcmp(client_json->valuestring, MQTT_CLIENT_IDENTIFIER) != 0)
    {
      ESP_LOGI(TAG_AWS_IOT, "Handling button event from %s", client_json->valuestring);
      if (strcmp(button_json->valuestring, MSG_BUTTON_PRESSED) == 0)
      {
        state_machine_post_event(APP_AWS_IOT_EVT_MSG_PRESSED, APP_AWS_IOT);
      }
      else if (strcmp(button_json->valuestring, MSG_BUTTON_RELEASED) == 0)
      {
        state_machine_post_event(APP_AWS_IOT_EVT_MSG_RELEASED, APP_AWS_IOT);
      }
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

  if (payload[0] != MQTTSubAckFailure)
  {
    ESP_LOGI(TAG_AWS_IOT, "Subscription to topic %s successful.", MQTT_TOPIC);
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
        // TODO - Handle PUBACK
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
static void aws_iot_on_init(GenericTask *self)
{
  MQTTStatus_t mqtt_status = MQTTSuccess;
  MQTTFixedBuffer_t network_buffer;
  TransportInterface_t transport = {0};

  transport.pNetworkContext = &network_context;
  transport.send = espTlsTransportSend;
  transport.recv = espTlsTransportRecv;
  transport.writev = NULL;

  network_buffer.pBuffer = buffer;
  network_buffer.size = MQTT_NETWORK_BUFFER_SIZE;

  mqtt_event_group = xEventGroupCreate();
  if (mqtt_event_group == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create MQTT event group");
    return;
  }

  mqtt_status = MQTT_Init(&mqtt_context,
                          &transport,
                          Clock_GetTimeMs,
                          aws_iot_event_callback,
                          &network_buffer);

  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT_Init failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    return;
  }

  mqtt_status = MQTT_InitStatefulQoS(&mqtt_context,
                                     outgoing_publish_records,
                                     MQTT_OUTGOING_PUBLISH_RECORD_LEN,
                                     incoming_publish_records,
                                     MQTT_INCOMING_PUBLISH_RECORD_LEN);

  if (mqtt_status != MQTTSuccess)
  {
    ESP_LOGE(TAG_AWS_IOT, "MQTT_InitStatefulQoS failed: Status = %s.", MQTT_Status_strerror(mqtt_status));
    return;
  }

  ESP_LOGI(TAG_AWS_IOT, "MQTT initialized successfully.");

  network_context.pcHostname = MQTT_BROKER_ENDPOINT;
  network_context.xPort = MQTT_PORT;
  network_context.pxTls = NULL;
  network_context.xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&tls_context_semaphore);
  network_context.disableSni = 0;

  if (network_context.xTlsContextSemaphore == NULL)
  {
    ESP_LOGE(TAG_AWS_IOT, "Failed to create TLS context semaphore.");
    return;
  }
  
  // Initialize TLS connection credentials
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
    return;
  }

  ESP_LOGI(TAG_AWS_IOT, "AWS IoT TLS credentials for client %s:", MQTT_CLIENT_IDENTIFIER);
  ESP_LOGI(TAG_AWS_IOT, "Using AmazonRootCA1.pem (%d bytes)",
           (int)network_context.pcServerRootCASize);
  ESP_LOGI(TAG_AWS_IOT, "Using device_certificate.pem.crt (%d bytes)",
           (int)network_context.pcClientCertSize);
  ESP_LOGI(TAG_AWS_IOT, "Using private_key.pem.key (%d bytes)",
           (int)network_context.pcClientKeySize);

  network_context.pAlpnProtos = NULL;

  connect_info.cleanSession = true;
  connect_info.pClientIdentifier = MQTT_CLIENT_IDENTIFIER;
  connect_info.clientIdentifierLength = MQTT_CLIENT_IDENTIFIER_LENGTH;
  connect_info.keepAliveSeconds = MQTT_KEEP_ALIVE_INTERVAL_S;

  ESP_LOGI(TAG_AWS_IOT, "AWS IoT Task initialized successfully.");
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
      aws_iot_connect_cmd();
      break;
    case APP_AWS_IOT_CMD_START_LISTENING:
      aws_iot_start_listening_cmd();
      break;
    case APP_AWS_IOT_CMD_PUBLISH_BUTTON_PRESSED:
      aws_iot_publish_button_event_cmd(MSG_BUTTON_PRESSED);
      break;
    case APP_AWS_IOT_CMD_PUBLISH_BUTTON_RELEASED:
      aws_iot_publish_button_event_cmd(MSG_BUTTON_RELEASED);
      break;
    default:
      break;
  }
}

/** @brief Create AWS IoT Task */
void aws_iot_task_init(void)
{
  aws_iot_task.name = TAG_AWS_IOT;
  aws_iot_task.on_init = aws_iot_on_init;
  aws_iot_task.on_message = aws_iot_on_message;
  aws_iot_task.item_size = sizeof(AwsIotMsg_t);
  generic_task_start(&aws_iot_task, MQTT_AWS_IOT_TASK_STACK_SIZE, 10);
}