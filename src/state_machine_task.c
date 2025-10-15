#include "state_machine_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "gpio_task.h"
#include "wifi_task.h"
#include "http_server.h"

#include "esp_log.h"

/** @enum eAppState_t
 *  @brief State Machine Application States
 */
typedef enum
{
  STATE_IDLE,
  STATE_PROVISIONING,
  STATE_CONNECTED,
} eAppState_t;

const char * TAG = "SM";

static QueueHandle_t sm_event_queue;
static httpd_handle_t sm_http_server = NULL;

/** @brief Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppEvent_t event)
{
  if (sm_event_queue != NULL)
  {
    return xQueueSend(sm_event_queue, &event, 0);
  }

  return errQUEUE_FULL;
}

/** @brief Transition to a new state
 *  @param new_state New state
 */
static void state_machine_enter_state(eAppState_t new_state)
{
  switch (new_state)
  {
    case STATE_IDLE:
      if (sm_http_server)
      {
        http_stop_webserver(sm_http_server);
        sm_http_server = NULL;
      }

      // TODO: read WiFi creds, decide STA vs AP
      wifi_set_ap_mode();
      break;

    case STATE_PROVISIONING:
      gpio_set_state(GPIO_STATE_LED_BLINK);
      sm_http_server = http_start_webserver();
      if (sm_http_server == NULL)
      {
        ESP_LOGE(TAG, "Failed to start HTTP server");
      }
      else
      {
        ESP_LOGI(TAG, "HTTP server started");
      }
      break;

    case STATE_CONNECTED:
      gpio_set_state(GPIO_STATE_LED_SOLID);
      if (sm_http_server)
      {
        http_stop_webserver(sm_http_server);
        sm_http_server = NULL;
      }
      break;
  }
}

/** @brief State Machine Task
 */
static void state_machine_task(void *args)
{
  ESP_LOGI(TAG, "State Machine Task Started");
  eAppEvent_t event;
  eAppState_t current_state = STATE_IDLE;
  state_machine_enter_state(current_state);

  while (true)
  {
    if (xQueueReceive(sm_event_queue, &event, portMAX_DELAY))
    {
      switch (current_state)
      {
        case STATE_IDLE:
          if (event == APP_EVENT_AP_STARTED)
          {
            ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_PROVISIONING);
            current_state = STATE_PROVISIONING;
            state_machine_enter_state(current_state);
          }
          else
          {
            ESP_LOGW(TAG, "Unexpected event %d received in STATE_IDLE", event);
          }
          break;

        case STATE_PROVISIONING:
          if (event == APP_EVENT_WIFI_CONNECTED)
          {
            ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_CONNECTED);
            current_state = STATE_CONNECTED;
            state_machine_enter_state(current_state);
          }
          else
          {
            ESP_LOGW(TAG, "Unexpected event %d received in STATE_PROVISIONING", event);
          }
          break;

        case STATE_CONNECTED:
          if (event == APP_EVENT_WIFI_DISCONNECTED)
          {
            ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_IDLE);
            current_state = STATE_IDLE;
            state_machine_enter_state(current_state);
          }
          else
          {
            ESP_LOGW(TAG, "Unexpected event %d received in STATE_CONNECTED", event);
          }
          break;
      }
    }
  }
}

/** @brief Create State Machine Task
 */
void state_machine_task_init()
{
  sm_event_queue = xQueueCreate(10, sizeof(eAppEvent_t));
  if (sm_event_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create event queue");
    return;
  }

  xTaskCreate(state_machine_task, TAG, 2048, NULL, 15, NULL);
  vTaskDelay(pdMS_TO_TICKS(1000)); // Give time for task to start
}