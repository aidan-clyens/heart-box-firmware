#include "application.h"

// FreeRTOS and ESP-IDF headers
#include "freertos/FreeRTOS.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

// Tasks
#include "file_system.h"
#include "wifi_task.h"
#include "gpio_task.h"
#include "aws_iot_task.h"
#include "state_machine_task.h"

const char *TAG = "APP";

/** @brief Initialize the application
 *  Initialize all necessary tasks for the application to run.
 *  Including Wifi, GPIO, AWS IoT, and the application State Machine.
 */
esp_err_t application_init(void)
{
  esp_err_t ret = ESP_OK;

  // Initialize global ESP-IDF networking and event loop (once per application)
  esp_netif_init();
  esp_event_loop_create_default();

  ESP_LOGI(TAG, "Network interface and event loop initialized");

  ret = file_system_init();
  if (ret != ESP_OK) {
    return ret;
  }

  ret = wifi_task_init();
  if (ret != ESP_OK) {
    return ret;
  }

  ret = gpio_task_init();
  if (ret != ESP_OK) {
    return ret;
  }

  ret = aws_iot_task_init();
  if (ret != ESP_OK) {
    return ret;
  }

  ret = state_machine_task_init();
  if (ret != ESP_OK) {
    return ret;
  }

  ESP_LOGI("APPLICATION", "Application initialized");
  return ret;
}

esp_err_t application_deinit(void)
{
  ESP_LOGI("APPLICATION", "Application deinitialized");
  return ESP_OK;
}