#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"

#include "script_helpers.h"

// Include the component header
#include "gpio_task.h"
#include "generic_task.h"

#define DELAY_TIME_MS 500

static const char *TAG = "TEST_GPIO_TASK";

// Test group setup
TEST_GROUP(gpio_task);
TEST_SETUP(gpio_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));
}

TEST_TEAR_DOWN(gpio_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_deinit());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));
}

/** @brief Test: Initialize GPIO task
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, initialize_task)
{
  unsigned int led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:initialize_task - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);
}

/** @brief Test: LED Solid State
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, led_solid)
{
  gpio_set_state(GPIO_STATE_LED_SOLID);

  vTaskDelay(pdMS_TO_TICKS(2000));

  unsigned int led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_solid - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);
}

/** @brief Test: LED Off State
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, led_off)
{
  gpio_set_state(GPIO_STATE_LED_OFF);

  vTaskDelay(pdMS_TO_TICKS(1000));

  unsigned int led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_off - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);
}

/** @brief Test: LED Blink State
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, led_blink)
{
  gpio_set_state(GPIO_STATE_LED_BLINK);

  unsigned int led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  vTaskDelay(pdMS_TO_TICKS(2.5 * GPIO_LED_BLINK_INTERVAL_MS));

  led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);

  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS));

  led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS));

  led_level = gpio_get_status_led_level();
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);
}

TEST(gpio_task, push_button_isr)
{
  gpio_simulate_button_press_isr();

  // The output LED is set to the button state
  // In this case, the button is set to LOW
  vTaskDelay(pdMS_TO_TICKS(500));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_output_led_level());

  // Simulate a button press to set it HIGH
  gpio_set_button_level(GPIO_HIGH);
  gpio_simulate_button_press_isr();

  // In this case, the button is set to HIGH
  vTaskDelay(pdMS_TO_TICKS(500));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_output_led_level());
}

TEST_GROUP_RUNNER(gpio_task)
{
  RUN_TEST_CASE(gpio_task, initialize_task);
  RUN_TEST_CASE(gpio_task, led_solid);
  RUN_TEST_CASE(gpio_task, led_off);
  RUN_TEST_CASE(gpio_task, led_blink);
  RUN_TEST_CASE(gpio_task, push_button_isr);
}
