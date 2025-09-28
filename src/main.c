#include "freertos/FREERTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <stdio.h>

#define HEART_LED_ARRAY_PIN GPIO_NUM_14
#define LED_STATUS_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_26

void initialize_gpio()
{
  gpio_reset_pin(HEART_LED_ARRAY_PIN);
  gpio_set_direction(HEART_LED_ARRAY_PIN, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN);
  gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);

  gpio_reset_pin(BUTTON_PIN);
  gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
}

void app_main()
{
  initialize_gpio();

  while (true)
  {
    printf("Turning LED ON\n");
    gpio_set_level(LED_STATUS_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Turning LED OFF\n");
    gpio_set_level(LED_STATUS_PIN, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}