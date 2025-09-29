#include "state_machine_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

static QueueHandle_t event_queue;
AppState_t current_state = STATE_STARTING;

/** @brief State Machine Starting State
 */
void state_machine_starting(AppEvent_t event)
{
  switch (event)
  {
  case EVENT_NO_WIFI_CREDENTIALS:
    ESP_LOGI("state_machine", "Transition from STARTING to REGISTRATION state");
    current_state = STATE_REGISRATION;
    break;
  case EVENT_WIFI_CREDENTIALS:
    ESP_LOGI("state_machine", "Transition from STARTING to INIT state");
    current_state = STATE_INIT;
  default:
    break;
  }
}

/** @brief State Machine Registration State
 */
void state_machine_registration(AppEvent_t event)
{
  switch (event)
  {
  case EVENT_WIFI_CONNECTED:
    ESP_LOGI("state_machine", "Transition from REGISTRATION to INIT state");
    current_state = STATE_INIT;
    break;
  default:
    break;
  }
}

/** @brief State Machine Task
 */
static void state_machine_task(void *args)
{
  AppEvent_t event;

  ESP_LOGI("state_machine", "State Machine Task Started");

  while (true)
  {
    if (xQueueReceive(event_queue, &event, portMAX_DELAY))
    {
      // Handle events and state transitions here
      ESP_LOGI("state_machine", "Event received: %d", event);
      switch (current_state)
      {
      case STATE_STARTING:
        state_machine_starting(event);
        break;
      case STATE_REGISRATION:
        state_machine_registration(event);
        break;
      case STATE_INIT:
        break;
      case STATE_RUNNING:
        break;
      default:
        break;
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/** @brief Create State Machine Task
 */
void state_machine_create_task()
{
  event_queue = xQueueCreate(10, sizeof(AppEvent_t));
  if (event_queue == NULL)
  {
    ESP_LOGE("state_machine", "Failed to create event queue");
    return;
  }

  xTaskCreate(state_machine_task, "state_machine_task", 2048, NULL, 10, NULL);
}