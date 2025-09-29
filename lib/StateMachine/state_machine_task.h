#ifndef __STATEMACHINE_TASK_H__
#define __STATEMACHINE_TASK_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

typedef enum
{
  STATE_STARTING,
  STATE_REGISRATION,
  STATE_INIT,
  STATE_RUNNING,
} AppState_t;

static QueueHandle_t event_queue;

/** @brief State Machine Task
 */
void state_machine_task(void *args)
{
  AppState_t current_state = STATE_STARTING;
  AppState_t event;

  ESP_LOGI("state_machine", "State Machine Task Started");

  while (true)
  {
    if (xQueueReceive(event_queue, &event, portMAX_DELAY))
    {
      // Handle events and state transitions here
      ESP_LOGI("state_machine", "Event received: %d", event);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/** @brief Create State Machine Task
 */
void state_machine_create_task()
{
  event_queue = xQueueCreate(10, sizeof(AppState_t));
  if (event_queue == NULL)
  {
    ESP_LOGE("state_machine", "Failed to create event queue");
    return;
  }

  xTaskCreate(state_machine_task, "state_machine_task", 2048, NULL, 10, NULL);
}

#endif // __STATEMACHINE_TASK_H__