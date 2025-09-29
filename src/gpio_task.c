#include "gpio_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

const char * TAG_BUTTON_TASK = "gpio_button_task";
const char * TAG_LED_BLINK_TASK = "gpio_led_blink_task";

SemaphoreHandle_t button_semaphore = NULL;

/** @brief Push Button Interrupt Service Routine
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(button_semaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

/** @brief Initialize GPIO
 */
void gpio_initialize()
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

  // Create binary semaphore
  button_semaphore = xSemaphoreCreateBinary();

  // Install ISR for button pin
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)BUTTON_PIN);
}

/** @brief GPIO Button Task
 */
static void gpio_button_task(void *args)
{
  ESP_LOGI(TAG_BUTTON_TASK, "GPIO Task Started");

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
      ESP_LOGI(TAG_BUTTON_TASK, "Button level: %d", button_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, button_level);

      gpio_intr_enable(BUTTON_PIN);
    }
  }
}

/** @brief GPIO LED Blink Task
 */
static void gpio_led_blink_task(void *arg)
{
  while (true)
  {
    ESP_LOGI(TAG_LED_BLINK_TASK, "Turning LED ON");
    gpio_set_level(LED_STATUS_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG_LED_BLINK_TASK, "Turning LED OFF");
    gpio_set_level(LED_STATUS_PIN, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/** @brief Create GPIO Tasks
 */
void gpio_task_init()
{
  gpio_initialize();

  xTaskCreate(gpio_button_task, TAG_BUTTON_TASK, 2048, NULL, 10, NULL);
  xTaskCreate(gpio_led_blink_task, TAG_LED_BLINK_TASK, 2048, NULL, 10, NULL);
}
