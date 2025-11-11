#include "script_helpers.h"

#include "unity.h"
#include "unity_fixture.h"

#include "wifi_task.h"
#include "gpio_task.h"
#include "aws_iot_task.h"
#include "state_machine_task.h"

void stop_all_tasks()
{
  if (aws_iot_task_is_running())
  {
    aws_iot_task_stop();
  }

  if (state_machine_task_is_running())
  {
    state_machine_task_stop();
  }

  if (wifi_task_is_running())
  {
    wifi_task_stop();
  }

  if (gpio_task_is_running())
  {
    gpio_task_stop();
  }

  vTaskDelay(pdMS_TO_TICKS(1000)); // Allow some time for tasks to stop

  TEST_ASSERT_FALSE(aws_iot_task_is_running());
  TEST_ASSERT_FALSE(state_machine_task_is_running());
  TEST_ASSERT_FALSE(wifi_task_is_running());
  TEST_ASSERT_FALSE(gpio_task_is_running());
}