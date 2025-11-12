#ifndef __GENERIC_TASK_H__
#define __GENERIC_TASK_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"

struct GenericTask;

// Callbacks now return esp_err_t to indicate success/failure
typedef esp_err_t (*TaskOnInit)(struct GenericTask *self);
typedef esp_err_t (*TaskOnStop)(struct GenericTask *self);
typedef void (*TaskOnMessage)(struct GenericTask *self, void *msg_buf, size_t msg_len);

typedef enum {
  TASK_STATE_STOPPED = 0,
  TASK_STATE_STARTING,
  TASK_STATE_RUNNING,
  TASK_STATE_STOPPING,
  TASK_STATE_ERROR
} eGenericTaskState;

typedef struct GenericTask
{
  const char *name;
  QueueHandle_t queue;
  TaskHandle_t handle;
  SemaphoreHandle_t state_mutex;
  eGenericTaskState state;
  size_t item_size;

  TaskOnInit on_init;
  TaskOnMessage on_message;
  TaskOnStop on_stop;
  
  // Internal flag for graceful shutdown
  volatile bool should_stop;
} GenericTask;

/** @brief Returns ESP_OK on success, error code on failure */
esp_err_t generic_task_start(GenericTask *task, uint32_t stack_size, UBaseType_t priority);

/** @brief Returns ESP_OK on success, error code on failure */
esp_err_t generic_task_stop(GenericTask *task);

BaseType_t generic_task_post_msg(GenericTask *task, const void *msg, size_t msg_len);

bool generic_task_is_running(GenericTask *task);
eGenericTaskState generic_task_get_state(GenericTask *task);

#endif // __GENERIC_TASK_H__