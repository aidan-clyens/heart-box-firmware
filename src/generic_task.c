#include "generic_task.h"
#include "esp_log.h"
#include <string.h>

static void generic_task_loop(void *arg)
{
  GenericTask *task = (GenericTask *)arg;

  if (task->on_init)
  {
    task->on_init(task);
  }

  uint8_t *msg_buf = pvPortMalloc(task->item_size);
  if (msg_buf == NULL)
  {
    ESP_LOGE(task->name, "Failed to allocate msg buffer of size %u", (unsigned)task->item_size);
    vTaskDelete(NULL);
    return;
  }

  for (;;)
  {
    if (xQueueReceive(task->queue, msg_buf, portMAX_DELAY))
    {
      if (task->on_message)
      {
        task->on_message(task, msg_buf, task->item_size);
      }
    }
  }
}

BaseType_t generic_task_post_msg(GenericTask *task, const void *msg, size_t msg_len)
{
  if (task->queue == NULL)
  {
    return errQUEUE_FULL;
  }
  if (msg_len != task->item_size)
  {
    // Enforce exact size to avoid truncation
    return errQUEUE_FULL;
  }
  return xQueueSend(task->queue, msg, pdMS_TO_TICKS(50));
}

void generic_task_start(GenericTask *task, uint32_t stack_size, UBaseType_t priority)
{
  if (task->item_size == 0)
  {
    ESP_LOGE(task->name, "item_size must be set before start");
    return;
  }

  task->queue = xQueueCreate(20, task->item_size);
  if (task->queue == NULL)
  {
    ESP_LOGE(task->name, "Failed to create message queue");
    return;
  }

  xTaskCreate(generic_task_loop, task->name, stack_size, task, priority, &task->handle);
}