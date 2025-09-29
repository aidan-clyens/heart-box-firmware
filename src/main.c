#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include <stdio.h>

#define HEART_LED_ARRAY_PIN GPIO_NUM_14
#define LED_STATUS_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

static SemaphoreHandle_t button_semaphore = NULL;

void initialize_gpio()
{
  gpio_reset_pin(HEART_LED_ARRAY_PIN);
  gpio_set_direction(HEART_LED_ARRAY_PIN, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN);
  gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);

  gpio_config_t button_pin_config = {
    .pin_bit_mask = (1ULL << BUTTON_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,     // disable internal pull-up
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // disable internal pull-down
    .intr_type = GPIO_INTR_ANYEDGE         // rising + falling edges
  };
  gpio_config(&button_pin_config);
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(button_semaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

static void push_button_handler_task(void *arg)
{
  int button_level = 0;

  while (true)
  {
    if (xSemaphoreTake(button_semaphore, portMAX_DELAY))
    {
      gpio_intr_disable(BUTTON_PIN);

      // Button debouncing
      vTaskDelay(BUTTON_DEBOUNCE_TIME_MS / portTICK_PERIOD_MS);

      // Check if the button is pressed or released
      button_level = gpio_get_level(BUTTON_PIN);
      printf("Button level: %d\n", button_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, button_level);

      gpio_intr_enable(BUTTON_PIN);
    }
  }
}

static void led_blink_task(void *arg)
{
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

void app_main()
{
  initialize_gpio();

  // Create binary semaphore
  button_semaphore = xSemaphoreCreateBinary();

  // Create GPIO tasks
  xTaskCreate(push_button_handler_task, "push_button_handler_task", 2048, NULL, 10, NULL);
  xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 10, NULL);

  // Install ISR for button pin
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*)BUTTON_PIN);
}