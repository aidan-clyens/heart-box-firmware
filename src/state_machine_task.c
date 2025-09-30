#include "state_machine_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "wifi_task.h"

#include "esp_log.h"

/** @enum AppState_t
 *  @brief State Machine Application States
 */
typedef enum
{
  STATE_STARTING,
  STATE_REGISRATION,
  STATE_WIFI_INIT,
  STATE_RUNNING,
} AppState_t;

const char * TAG = "state_machine_task";

static QueueHandle_t state_machine_event_queue;
AppState_t current_state = STATE_STARTING;

/** @brief Post an event to the State Machine task
 *  @param event The event to post
 */
void state_machine_post_event(eAppEvent_t event)
{
  if (state_machine_event_queue != NULL)
  {
    xQueueSend(state_machine_event_queue, &event, portMAX_DELAY);
  }
}

/** @brief State Machine Starting State
 */
void state_machine_starting()
{
  ESP_LOGI(TAG, "Changed to STARTING state");

  // TODO - Read NVS to check if WiFi credentials are available
  bool wifi_credentials_available = true;

  current_state = (wifi_credentials_available) ? STATE_WIFI_INIT : STATE_REGISRATION;
}

/** @brief State Machine Registration State
 */
void state_machine_registration()
{
  ESP_LOGI(TAG, "Changed to REGISTRATION state");
  // Send event to WiFi task to start in AP+STA mode
  wifi_post_cmd(WIFI_CMD_MODE_AP_STA);

  // TODO - Wait for WiFi event
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

/** @brief State Machine Initialization State
 */
void state_machine_init()
{
  ESP_LOGI(TAG, "Changed to INIT state");
  // Send event to WiFi task to start connection in STA mode
  wifi_post_cmd(WIFI_CMD_SET_STA_CREDENTIALS);

  // Wait for WiFi connection event
  while (true)
  {
    eAppEvent_t event;
    if (xQueueReceive(state_machine_event_queue, &event, portMAX_DELAY))
    {
      if (event == APP_EVENT_WIFI_CONNECTED)
      {
        ESP_LOGI(TAG, "Received WIFI_CONNECTED event");
        ESP_LOGI(TAG, "Changed to RUNNING state");
        current_state = STATE_RUNNING;
        break;
      }
    }
  }
}

/** @brief State Machine Task
 */
static void state_machine_task(void *args)
{
  ESP_LOGI(TAG, "State Machine Task Started");

  while (true)
  {
    switch (current_state)
    {
    case STATE_STARTING:
      state_machine_starting();
      break;
    case STATE_REGISRATION:
      state_machine_registration();
      break;
    case STATE_WIFI_INIT:
      state_machine_init();
      break;
    case STATE_RUNNING:
      while (true)
      {
        // In RUNNING state, periodically ping to check connectivity
        wifi_post_cmd(WIFI_CMD_PING);
        vTaskDelay(pdMS_TO_TICKS(30000));
      }
    default:
      break;
    }
  }
}

/** @brief Create State Machine Task
 */
void state_machine_task_init()
{
  state_machine_event_queue = xQueueCreate(10, sizeof(eAppEvent_t));
  if (state_machine_event_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create event queue");
    return;
  }

  xTaskCreate(state_machine_task, TAG, 2048, NULL, 10, NULL);
  vTaskDelay(pdMS_TO_TICKS(1000)); // Give time for task to start
}