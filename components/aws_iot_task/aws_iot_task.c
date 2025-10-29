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

#define NETWORK_BUFFER_SIZE 2048
#define MQTT_BROKER_ENDPOINT "airgahux2exxu-ats.iot.us-east-1.amazonaws.com"
#define MQTT_PORT 8883

// --- Module State ---
static const char *TAG_AWS_IOT = "AWS_IOT_TASK";
static GenericTask aws_iot_task;

static MQTTContext_t mqtt_context = {0};
static NetworkContext_t network_context = {0};

static uint8_t buffer[NETWORK_BUFFER_SIZE];

static StaticSemaphore_t tls_context_semaphore;

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

static void aws_iot_connect_cmd(void)
{
  ESP_LOGI(TAG_AWS_IOT, "Connecting to AWS IoT broker...");

  network_context.pcHostname = MQTT_BROKER_ENDPOINT;
  network_context.xPort = MQTT_PORT;
  network_context.pxTls = NULL;
  network_context.xTlsContextSemaphore = xSemaphoreCreateMutexStatic(&tls_context_semaphore);
  network_context.disableSni = 0;

  // Initialize TLS connection
  network_context.pcServerRootCA = NULL; // Set root CA certificate
  network_context.pcServerRootCASize = NULL;
}

static void aws_iot_event_callback(MQTTContext_t * p_mqtt_context,
                                   MQTTPacketInfo_t * p_packet_info,
                                   MQTTDeserializedInfo_t * p_deserialized_info)
{
  uint16_t packet_id = p_deserialized_info->packetIdentifier;

  if ((p_packet_info->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
  {
    // TODO - Handle incoming publish
  }
  else
  {
    switch (p_packet_info->type)
    {
      case MQTT_PACKET_TYPE_SUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received SUBACK MQTT packet");
        // TODO - Handle SUBACK
        break;

      case MQTT_PACKET_TYPE_UNSUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received UNSUBACK MQTT packet");
        // TODO - Handle UNSUBACK
        break;

      case MQTT_PACKET_TYPE_PUBACK:
        ESP_LOGI(TAG_AWS_IOT, "Received PUBACK MQTT packet");
        // TODO - Handle PUBACK
        break;

      case MQTT_PACKET_TYPE_PINGRESP:
        ESP_LOGI(TAG_AWS_IOT, "Received PINGRESP MQTT packet");
        // TODO - Handle PINGRESP
        break;

      case MQTT_PACKET_TYPE_CONNACK:
        ESP_LOGI(TAG_AWS_IOT, "Received CONNACK MQTT packet");
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
  network_buffer.size = NETWORK_BUFFER_SIZE;

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
  generic_task_start(&aws_iot_task, 4096, 10);
}