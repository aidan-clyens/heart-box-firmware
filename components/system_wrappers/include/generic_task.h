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

/** @brief Public API: Creates a new GenericTask instance
 *  @param name Name of the task
 *  @param item_size Size of each message item in the queue
 *  @param on_init Callback function for task initialization. May be NULL.
 *  @param on_message Callback function for handling messages. May be NULL.
 *  @param on_stop Callback function for task cleanup. May be NULL.
 *  @return Pointer to the created GenericTask instance, or NULL on failure
 */
GenericTask *generic_task_create(
    const char *name,
    size_t item_size,
    TaskOnInit on_init,
    TaskOnMessage on_message,
    TaskOnStop on_stop);

esp_err_t generic_task_delete(GenericTask *task);

/** @brief Public API: Returns ESP_OK on success, error code on failure
 *  @param task Pointer to the GenericTask instance
 *  @param stack_size Stack size for the FreeRTOS task
 *  @param priority Priority for the FreeRTOS task
 *  @return ESP_OK on success, error code on failure
*/
esp_err_t generic_task_start(GenericTask *task, uint32_t stack_size, UBaseType_t priority);

/** @brief Returns ESP_OK on success, error code on failure */
esp_err_t generic_task_stop(GenericTask *task);

/** @brief Public API: Posts a message to the task's message queue
 *  @param task Pointer to the GenericTask instance
 *  @param msg Pointer to the message buffer
 *  @param msg_len Length of the message in bytes
 *  @return pdTRUE if the message was posted successfully, pdFALSE otherwise
 */
BaseType_t generic_task_post_msg(GenericTask *task, const void *msg, size_t msg_len);

/** @brief Public API: Checks if the task is currently running
 *  @param task Pointer to the GenericTask instance
 *  @return true if the task is running, false otherwise
 */
bool generic_task_is_running(GenericTask *task);

/** @brief Public API: Gets the current state of the task
 *  @param task Pointer to the GenericTask instance
 *  @return Current state of the task as eGenericTaskState
 */
eGenericTaskState generic_task_get_state(GenericTask *task);

#endif // __GENERIC_TASK_H__