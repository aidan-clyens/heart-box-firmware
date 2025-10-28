#include "mqtt_task.h"
#include "generic_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "mqtt_client.h"

// --- Module State ---
static const char *TAG_MQTT = "MQTT";
static GenericTask mqtt_task;
static esp_mqtt_client_handle_t mqtt_client = NULL;

static const char *MQTT_BROKER_URL =
    "airgahux2exxu-ats.iot.us-east-1.amazonaws.com";

/** @brief Post a command message to the MQTT task
 *  @param msg The command message to post
 */
BaseType_t mqtt_post_msg(MqttMsg_t msg)
{
  return generic_task_post_msg(&mqtt_task, &msg, sizeof(MqttMsg_t));
}

/** @brief Request a connection to the configured MQTT broker
 */
void mqtt_connect(void)
{
  MqttMsg_t msg = {.type = APP_MQTT_CMD_CONNECT};
  mqtt_post_msg(msg);
}

/** @brief Request a disconnect from the MQTT broker
 */
void mqtt_disconnect(void)
{
  MqttMsg_t msg = {.type = APP_MQTT_CMD_DISCONNECT};
  mqtt_post_msg(msg);
}

/** @brief Event handler for MQTT client events
 *  @param handler_args User defined argument
 *  @param base         Event base
 *  @param event_id     Event ID
 *  @param event_data   Event data
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = event_data;
  MqttMsg_t msg = {0};

  switch ((esp_mqtt_event_id_t)event_id)
  {
  case MQTT_EVENT_CONNECTED:
    msg.type = APP_MQTT_EVT_CONNECTED;
    break;
  case MQTT_EVENT_DISCONNECTED:
    msg.type = APP_MQTT_EVT_DISCONNECTED;
    break;
  case MQTT_EVENT_ERROR:
    msg.type = APP_MQTT_EVT_ERROR;
    msg.data.error_code = event->error_handle->esp_tls_last_esp_err;
    break;
  default:
    return;
  }

  mqtt_post_msg(msg);
}

/** @brief Initialize MQTT task state
 *  @param self Pointer to the GenericTask
 */
static void mqtt_on_init(GenericTask *self)
{
  ESP_LOGI(TAG_MQTT, "MQTT Task initialized");
}

/** @brief MQTT Task message handler
 *  @param self    Pointer to the GenericTask
 *  @param msg_buf Pointer to the received message buffer
 *  @param msg_len Length of the message buffer
 */
static void mqtt_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  if (msg_len != sizeof(MqttMsg_t))
  {
    return;
  }

  MqttMsg_t *msg = (MqttMsg_t *)msg_buf;

  switch (msg->type)
  {
  case APP_MQTT_CMD_CONNECT:
  {
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
    };
    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG_MQTT, "Connecting to broker...");
    break;
  }

  case APP_MQTT_CMD_DISCONNECT:
    if (mqtt_client)
    {
      esp_mqtt_client_stop(mqtt_client);
      esp_mqtt_client_destroy(mqtt_client);
      mqtt_client = NULL;
      ESP_LOGI(TAG_MQTT, "Disconnected");
    }
    break;

  case APP_MQTT_EVT_CONNECTED:
    ESP_LOGI(TAG_MQTT, "MQTT Connected");
    break;

  case APP_MQTT_EVT_DISCONNECTED:
    ESP_LOGI(TAG_MQTT, "MQTT Disconnected");
    break;

  case APP_MQTT_EVT_ERROR:
    ESP_LOGE(TAG_MQTT, "MQTT Error: %d", msg->data.error_code);
    break;

  default:
    ESP_LOGW(TAG_MQTT, "Unknown MQTT message %d", msg->type);
    break;
  }
}

/** @brief Create MQTT Task
 */
void mqtt_task_init(void)
{
  mqtt_task.name = TAG_MQTT;
  mqtt_task.on_init = mqtt_on_init;
  mqtt_task.on_message = mqtt_on_message;
  mqtt_task.item_size = sizeof(MqttMsg_t);
  generic_task_start(&mqtt_task, 4096, 10);
}