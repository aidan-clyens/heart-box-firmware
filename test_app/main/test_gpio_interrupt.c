#include "unity.h"
#include "unity_fixture.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

#include "esp_log.h"

#define LED_STATUS_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_25

#define GPIO_HIGH 1
#define GPIO_LOW 0

#define BUTTON_DEBOUNCE_TIME_MS 50

static const char *TAG = "GPIO_TASK_TEST";

static SemaphoreHandle_t gpio_button_semaphore = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(gpio_button_semaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

// Test group setup
TEST_GROUP(gpio_interrupt);
TEST_SETUP(gpio_interrupt)
{
  gpio_reset_pin(LED_STATUS_PIN);
  gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_STATUS_PIN, GPIO_LOW);

  gpio_config_t button_pin_config = {
      .pin_bit_mask = (1ULL << BUTTON_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE};

  TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_config(&button_pin_config));

  gpio_button_semaphore = xSemaphoreCreateBinary();
  TEST_ASSERT_NOT_NULL_MESSAGE(gpio_button_semaphore, "Failed to create button semaphore");
  TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_install_isr_service(0));
  TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)BUTTON_PIN));

  gpio_set_level(LED_STATUS_PIN, GPIO_HIGH);

  ESP_LOGI(TAG, "GPIO Interrupt Test Setup Complete");
}

TEST_TEAR_DOWN(gpio_interrupt)
{
  TEST_ASSERT_EQUAL_INT(ESP_OK, gpio_isr_handler_remove(BUTTON_PIN));

  gpio_uninstall_isr_service();
  vSemaphoreDelete(gpio_button_semaphore);
  gpio_button_semaphore = NULL;

  ESP_LOGI(TAG, "GPIO Interrupt Test Teardown Complete");
}

TEST(gpio_interrupt, debounce_value_comparison)
{
  ESP_LOGI(TAG, "Starting GPIO Interrupt Test Case");

  int debounce_delay_ms = 0;
  int debounce_delay_increment_ms = 10;
  int max_debounce_delay_ms = 90;
  int press_count = 0;
  int expected_presses = 3;

  int event_count = 0;
  int expected_events = 6; // Each press generates 2 events (press and release)

  for (int debounce = 30; debounce <= max_debounce_delay_ms; debounce += debounce_delay_increment_ms)
  {
    ESP_LOGI(TAG, "-- Testing with debounce delay: %d ms ---", debounce);
    ESP_LOGI(TAG, "-- Press the button connected to GPIO %d %d times --", BUTTON_PIN, expected_presses);
    debounce_delay_ms = debounce;

    press_count = 0;
    event_count = 0;
    while (event_count < expected_events)
    {
      if (xSemaphoreTake(gpio_button_semaphore, portMAX_DELAY))
      {
        gpio_intr_disable(BUTTON_PIN);

        vTaskDelay(pdMS_TO_TICKS(debounce_delay_ms));

        // Clear any pending events that arrived during debounce
        while (xSemaphoreTake(gpio_button_semaphore, 0) == pdTRUE) {
          // Drain the semaphore
        }

        event_count++;

        int level = gpio_get_level(BUTTON_PIN);
        if (level == GPIO_HIGH)
        {
          press_count++;
        }

        ESP_LOGI(TAG, "Received Push Button Event (%d/%d) - Button level: %d", event_count, expected_events, level);
        gpio_set_level(LED_STATUS_PIN, level);

        gpio_intr_enable(BUTTON_PIN);
      }
    }
    ESP_LOGI(TAG, "-- Completed %d button presses with debounce delay %d ms --", press_count, debounce_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

TEST_GROUP_RUNNER(gpio_interrupt)
{
  RUN_TEST_CASE(gpio_interrupt, debounce_value_comparison);
}
