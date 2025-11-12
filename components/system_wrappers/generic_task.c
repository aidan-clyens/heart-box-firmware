#include "generic_task.h"
#include "esp_log.h"
#include <string.h>

#define STOP_TIMEOUT_MS 5000

static void generic_task_loop(void *arg)
{
  GenericTask *task = (GenericTask *)arg;
  esp_err_t init_result = ESP_OK;

  if (task->on_init)
  {
    init_result = task->on_init(task);
    if (init_result != ESP_OK)
    {
      ESP_LOGE(task->name, "on_init failed: %s", esp_err_to_name(init_result));
      
      // Signal that initialization failed
      xSemaphoreTake(task->state_mutex, portMAX_DELAY);
      task->state = TASK_STATE_ERROR;
      xSemaphoreGive(task->state_mutex);
      
      vTaskDelete(NULL);
      return;
    }
  }

  // Mark as running after successful init
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  task->state = TASK_STATE_RUNNING;
  xSemaphoreGive(task->state_mutex);

  uint8_t *msg_buf = pvPortMalloc(task->item_size);
  if (msg_buf == NULL)
  {
    ESP_LOGE(task->name, "Failed to allocate msg buffer of size %u", (unsigned)task->item_size);
    
    xSemaphoreTake(task->state_mutex, portMAX_DELAY);
    task->state = TASK_STATE_ERROR;
    xSemaphoreGive(task->state_mutex);
    
    vTaskDelete(NULL);
    return;
  }

  // Main message loop
  for (;;)
  {
    // Check for stop signal
    if (task->should_stop)
    {
      break;
    }

    // Wait for messages with timeout to periodically check should_stop
    if (xQueueReceive(task->queue, msg_buf, pdMS_TO_TICKS(100)))
    {
      if (task->on_message)
      {
        task->on_message(task, msg_buf, task->item_size);
      }
    }
  }

  // Cleanup
  vPortFree(msg_buf);
  
  vTaskDelete(NULL);
}

esp_err_t generic_task_start(GenericTask *task, uint32_t stack_size, UBaseType_t priority)
{
  if (!task)
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Take mutex to prevent concurrent start/stop
  if (!task->state_mutex)
  {
    task->state_mutex = xSemaphoreCreateMutex();
    if (!task->state_mutex)
    {
      ESP_LOGE(task->name, "Failed to create state mutex");
      return ESP_ERR_NO_MEM;
    }
  }

  xSemaphoreTake(task->state_mutex, portMAX_DELAY);

  // Check if already running or in transition
  if (task->state != TASK_STATE_STOPPED && task->state != TASK_STATE_ERROR)
  {
    ESP_LOGW(task->name, "Task already running or in transition (state=%d)", task->state);
    xSemaphoreGive(task->state_mutex);
    return ESP_ERR_INVALID_STATE;
  }

  if (task->item_size == 0)
  {
    ESP_LOGE(task->name, "item_size must be set before start");
    xSemaphoreGive(task->state_mutex);
    return ESP_ERR_INVALID_ARG;
  }

  // Mark as starting
  task->state = TASK_STATE_STARTING;
  task->should_stop = false;
  
  xSemaphoreGive(task->state_mutex);

  // Step 1: Create queue
  QueueHandle_t new_queue = xQueueCreate(20, task->item_size);
  if (new_queue == NULL)
  {
    ESP_LOGE(task->name, "Failed to create message queue");
    
    xSemaphoreTake(task->state_mutex, portMAX_DELAY);
    task->state = TASK_STATE_STOPPED;
    xSemaphoreGive(task->state_mutex);
    
    return ESP_ERR_NO_MEM;
  }

  // Step 2: Create FreeRTOS task
  TaskHandle_t new_handle = NULL;
  BaseType_t create_result = xTaskCreate(
    generic_task_loop, 
    task->name, 
    stack_size, 
    task, 
    priority, 
    &new_handle
  );

  if (create_result != pdPASS || new_handle == NULL)
  {
    ESP_LOGE(task->name, "Failed to create FreeRTOS task");
    
    // Rollback: Delete queue
    vQueueDelete(new_queue);
    
    xSemaphoreTake(task->state_mutex, portMAX_DELAY);
    task->state = TASK_STATE_STOPPED;
    xSemaphoreGive(task->state_mutex);
    
    return ESP_ERR_NO_MEM;
  }

  // Commit the changes
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  task->queue = new_queue;
  task->handle = new_handle;
  // State will be set to RUNNING by the task itself after on_init succeeds
  // or to ERROR if on_init fails
  xSemaphoreGive(task->state_mutex);

  // Wait briefly for task to initialize and update state
  vTaskDelay(pdMS_TO_TICKS(100));

  // Check if initialization succeeded
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  eGenericTaskState final_state = task->state;
  xSemaphoreGive(task->state_mutex);

  if (final_state == TASK_STATE_ERROR)
  {
    ESP_LOGE(task->name, "Task initialization failed");
    
    // Clean up since task deleted itself
    xSemaphoreTake(task->state_mutex, portMAX_DELAY);
    if (task->queue)
    {
      vQueueDelete(task->queue);
      task->queue = NULL;
    }
    task->handle = NULL;
    task->state = TASK_STATE_STOPPED;
    xSemaphoreGive(task->state_mutex);
    
    return ESP_FAIL;
  }

  ESP_LOGI(task->name, "Task started successfully");
  return ESP_OK;
}

esp_err_t generic_task_stop(GenericTask *task)
{
  if (!task)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (!task->state_mutex)
  {
    ESP_LOGW(task->name, "Task was never started");
    return ESP_ERR_INVALID_STATE;
  }

  xSemaphoreTake(task->state_mutex, portMAX_DELAY);

  // Check if task is running
  if (task->state != TASK_STATE_RUNNING)
  {
    ESP_LOGW(task->name, "Task not running (state=%d)", task->state);
    xSemaphoreGive(task->state_mutex);
    return ESP_ERR_INVALID_STATE;
  }

  // Mark as stopping
  task->state = TASK_STATE_STOPPING;
  TaskHandle_t handle_backup = task->handle;
  QueueHandle_t queue_backup = task->queue;
  
  xSemaphoreGive(task->state_mutex);

  // Call on_stop callback before deleting task
  esp_err_t stop_result = ESP_OK;
  if (task->on_stop)
  {
    stop_result = task->on_stop(task);
    if (stop_result != ESP_OK)
    {
      ESP_LOGE(task->name, "on_stop failed: %s", esp_err_to_name(stop_result));
      
      // Rollback to running state
      xSemaphoreTake(task->state_mutex, portMAX_DELAY);
      task->state = TASK_STATE_RUNNING;
      xSemaphoreGive(task->state_mutex);
      
      return stop_result;
    }
  }

  // Signal task to stop gracefully
  task->should_stop = true;

  // Wait for task to finish (with timeout)
  uint32_t wait_count = 0;
  while (eTaskGetState(handle_backup) != eDeleted && wait_count < STOP_TIMEOUT_MS / 10)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
    wait_count++;
  }

  if (eTaskGetState(handle_backup) != eDeleted)
  {
    ESP_LOGW(task->name, "Task didn't stop gracefully, forcing deletion");
    vTaskDelete(handle_backup);
  }

  // Clean up resources
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  
  if (task->queue != NULL)
  {
    vQueueDelete(task->queue);
    task->queue = NULL;
  }
  
  task->handle = NULL;
  task->state = TASK_STATE_STOPPED;
  task->should_stop = false;
  
  xSemaphoreGive(task->state_mutex);

  ESP_LOGI(task->name, "Task stopped successfully");
  return ESP_OK;
}

BaseType_t generic_task_post_msg(GenericTask *task, const void *msg, size_t msg_len)
{
  if (!task || task->queue == NULL)
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

bool generic_task_is_running(GenericTask *task)
{
  if (!task || !task->state_mutex)
  {
    return false;
  }
  
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  bool running = (task->state == TASK_STATE_RUNNING);
  xSemaphoreGive(task->state_mutex);
  
  return running;
}

eGenericTaskState generic_task_get_state(GenericTask *task)
{
  if (!task || !task->state_mutex)
  {
    return TASK_STATE_STOPPED;
  }
  
  xSemaphoreTake(task->state_mutex, portMAX_DELAY);
  eGenericTaskState state = task->state;
  xSemaphoreGive(task->state_mutex);
  
  return state;
}