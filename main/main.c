#include "freertos/FreeRTOS.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "state_machine_task.h"
#include "gpio_task.h"
#include "wifi_task.h"
#include "aws_iot_task.h"
#include "file_system.h"

static const char *TAG = "MAIN";

/** MAIN **/

/** @brief Main Task
 */
void app_main()
{
  // Initialize global ESP-IDF networking and event loop (once per application)
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_LOGI(TAG, "Network interface and event loop initialized");

  file_system_init();

  // Create tasks
  wifi_task_init();
  gpio_task_init();
  aws_iot_task_init();
  state_machine_task_init();
}