#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"

// Include the component header
#include "gpio_task.h"

static const char *TAG = "TEST_GPIO_TASK";

// Test group setup
TEST_GROUP(gpio_task);
TEST_SETUP(gpio_task)
{
  // Setup code here
}

TEST_TEAR_DOWN(gpio_task)
{
  // Teardown code here
}

/** @brief Test: Initialize GPIO task
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, initialize_task)
{
  gpio_task_init();

  vTaskDelay(pdMS_TO_TICKS(1000));

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

TEST_GROUP_RUNNER(gpio_task)
{
  RUN_TEST_CASE(gpio_task, initialize_task);
  RUN_TEST_CASE(gpio_task, led_solid);
  RUN_TEST_CASE(gpio_task, led_off);
  RUN_TEST_CASE(gpio_task, led_blink);
}
