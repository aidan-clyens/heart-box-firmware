#include "script_helpers.h"

#include "unity.h"
#include "unity_fixture.h"

#include "wifi_task.h"
#include "gpio_task.h"
#include "aws_iot_task.h"
#include "state_machine_task.h"

void stop_all_tasks()
{
  wifi_task_deinit();
  gpio_task_deinit();
}