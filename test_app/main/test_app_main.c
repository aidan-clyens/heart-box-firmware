#include "unity.h"
#include "unity_fixture.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "file_system.h"

static const char *TAG = "TEST_APP";

extern void TEST_state_machine_task_GROUP_RUNNER(void);
extern void TEST_file_system_GROUP_RUNNER(void);
extern void TEST_gpio_task_GROUP_RUNNER(void);
extern void TEST_wifi_task_GROUP_RUNNER(void);
extern void TEST_aws_iot_task_GROUP_RUNNER(void);
extern void TEST_generic_task_GROUP_RUNNER(void);
extern void TEST_gpio_interrupt_GROUP_RUNNER(void);

static void run_all_tests(void)
{
  // RUN_TEST_GROUP(generic_task);
  // RUN_TEST_GROUP(file_system);
  // RUN_TEST_GROUP(gpio_task);
  // RUN_TEST_GROUP(wifi_task);
  RUN_TEST_GROUP(aws_iot_task);
  // RUN_TEST_GROUP(state_machine_task);
  // RUN_TEST_GROUP(gpio_interrupt);
}

// Test application main
void app_main(void)
{
  // Initialize global ESP-IDF networking and event loop (once per application)
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_LOGI(TAG, "Network interface and event loop initialized for tests");

  file_system_init();

  UNITY_MAIN_FUNC(run_all_tests);
}