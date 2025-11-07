#include "freertos/FreeRTOS.h"

#include "state_machine_task.h"
#include "gpio_task.h"
#include "wifi_task.h"
#include "aws_iot_task.h"
#include "file_system.h"

/** MAIN **/

/** @brief Main Task
 */
void app_main()
{
  file_system_init();

  // Create tasks
  wifi_task_init();
  gpio_task_init();
  aws_iot_task_init();
  state_machine_task_init();
}