#include "state_machine_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "wifi_task.h"
#include "http_server.h"

#include "esp_log.h"

/** @enum AppState_t
 *  @brief State Machine Application States
 */
typedef enum
{
  STATE_IDLE,
  STATE_PROVISIONING,
  STATE_CONNECTED,
} AppState_t;

const char * TAG = "state_machine_task";

static QueueHandle_t state_machine_event_queue;
AppState_t current_state = STATE_IDLE;

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

/** @brief Change the current state
 *  @param new_state The new state to change to
 */
void state_machine_change_state(AppState_t new_state)
{
  ESP_LOGI(TAG, "State changed from %d to %d", current_state, new_state);
  current_state = new_state;
}

/** @brief State Machine Task
 */
static void state_machine_task(void *args)
{
  ESP_LOGI(TAG, "State Machine Task Started");
  eAppEvent_t event;
  httpd_handle_t http_server = NULL;

  while (true)
  {
    switch (current_state)
    {
    case STATE_IDLE:
      // TODO - Read Wifi credentials from NVS
      // TODO - If credentials exist, post command to WiFi task to connect in STA mode

      // If no credentials, post command to WiFi task to start AP mode
      wifi_set_ap_mode();
      if (xQueueReceive(state_machine_event_queue, &event, portMAX_DELAY))
      {
        if (event == APP_EVENT_AP_STARTED)
        {
          state_machine_change_state(STATE_PROVISIONING);
        }
      }
      break;
    case STATE_PROVISIONING:
      // Start HTTP server for provisioning
      http_server = http_start_webserver();
      if (http_server == NULL)
      {
        ESP_LOGE(TAG, "Failed to start HTTP server");
      }
      else
      {
        ESP_LOGI(TAG, "HTTP server started");
      }

      while (true)
      {
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
      break;
    case STATE_CONNECTED:
      break;
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