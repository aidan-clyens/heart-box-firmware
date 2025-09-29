#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "state_machine_task.h"
#include "gpio_task.h"
#include "wifi_task.h"

#include <stdio.h>

/** @brief Initialize Non-Volatile Storage (NVS)
 */
void nvs_initialize()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

/** MAIN **/

/** @brief Main Task
 */
void app_main()
{
  nvs_initialize();

  gpio_initialize();
  wifi_initialize();

  // Create tasks
  state_machine_create_task();
  gpio_create_task();
}