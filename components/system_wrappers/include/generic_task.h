#ifndef __GENERIC_TASK_H__
#define __GENERIC_TASK_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

struct GenericTask;

typedef void (*TaskOnInit)(struct GenericTask *self);
typedef void (*TaskOnStop)(struct GenericTask *self);
typedef void (*TaskOnMessage)(struct GenericTask *self, void *msg_buf, size_t msg_len);

typedef struct GenericTask
{
  const char *name;
  QueueHandle_t queue;
  TaskHandle_t handle;
  size_t item_size;

  TaskOnInit on_init;
  TaskOnMessage on_message;
  TaskOnStop on_stop;
} GenericTask;

BaseType_t generic_task_post_msg(GenericTask *task, const void *msg, size_t msg_len);
void generic_task_start(GenericTask *task, uint32_t stack_size, UBaseType_t priority);
void generic_task_stop(GenericTask *task);

bool generic_task_is_running(GenericTask *task);

#endif // __GENERIC_TASK_H__