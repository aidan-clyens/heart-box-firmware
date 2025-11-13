#include "script_helpers.h"

#include "unity.h"
#include "unity_fixture.h"

#include "wifi_task.h"

void wait_for_connection(unsigned int timeout_ms, bool expect_success)
{
  TickType_t start_tick = xTaskGetTickCount();
  while (xTaskGetTickCount() - start_tick < pdMS_TO_TICKS(timeout_ms))
  {
    if (wifi_task_is_connected())
    {
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (expect_success) {
    TEST_FAIL_MESSAGE("Timeout waiting for WiFi connection state change");
  }
}