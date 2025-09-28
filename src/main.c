#include "freertos/FREERTOS.h"
#include "freertos/task.h"
#include <stdio.h>

void app_main()
{
  while (true)
  {
    printf("Hello, FreeRTOS!\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}