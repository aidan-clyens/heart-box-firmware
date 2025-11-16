#include "unity.h"
#include "unity_fixture.h"

#include "esp_log.h"
#include "esp_heap_caps.h"

// Include the component header
#include "gpio_task.h"
#include "generic_task.h"

#define DELAY_TIME_MS 500
#define HEAP_TOLERANCE_BYTES 100 // Allow small variance

static const char *TAG = "TEST_GPIO_TASK";

static size_t initial_free_heap_size = 0;

// Test group setup
TEST_GROUP(gpio_task);
TEST_SETUP(gpio_task)
{
  initial_free_heap_size = esp_get_free_heap_size();
  ESP_LOGI(TAG, "Initial free heap size: %u bytes", initial_free_heap_size);

  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_init());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));
}

TEST_TEAR_DOWN(gpio_task)
{
  TEST_ASSERT_EQUAL(ESP_OK, gpio_task_deinit());
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  // Check heap for memory leaks
  size_t final_free_heap_size = esp_get_free_heap_size();
  ESP_LOGI(TAG, "Final free heap size: %u bytes", final_free_heap_size);

  int heap_diff = final_free_heap_size - initial_free_heap_size;
  ESP_LOGI(TAG, "Heap size difference: %d bytes", heap_diff);

  if (heap_diff < -HEAP_TOLERANCE_BYTES)
  {
    ESP_LOGE(TAG, "Memory leak detected! Lost %d bytes (tolerance: %d)",
             -heap_diff, HEAP_TOLERANCE_BYTES);
  }
  else if (heap_diff > HEAP_TOLERANCE_BYTES)
  {
    ESP_LOGW(TAG, "Heap grew unexpectedly by %d bytes (tolerance: %d)",
             heap_diff, HEAP_TOLERANCE_BYTES);
  }
  else
  {
    ESP_LOGI(TAG, "Heap check passed (within ±%d byte tolerance)", HEAP_TOLERANCE_BYTES);
  }

  TEST_ASSERT_GREATER_OR_EQUAL_INT(-HEAP_TOLERANCE_BYTES, heap_diff);
}

/** @brief Test Helper: LED Solid State
 *  @param led_pin The LED GPIO pin to test
 */
static void test_led_solid_state(int led_pin)
{
  gpio_set_state(led_pin, GPIO_STATE_LED_SOLID);

  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  unsigned int led_level = gpio_get_led_level(led_pin);
  ESP_LOGI(TAG, "gpio_task:led_solid - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);

  gpio_set_state(led_pin, GPIO_STATE_LED_OFF);
  vTaskDelay(pdMS_TO_TICKS(DELAY_TIME_MS));

  led_level = gpio_get_led_level(led_pin);
  ESP_LOGI(TAG, "gpio_task:led_off - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);
}

/** @brief Test Helper: LED Blink State
 *  @param pin The LED GPIO pin to test
 */
static void test_led_blink_state(int pin)
{
  gpio_set_state(pin, GPIO_STATE_LED_BLINK);

  // Verify initial state is LOW
  vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to allow state to update
  unsigned int led_level = gpio_get_led_level(pin);
  ESP_LOGI(TAG, "gpio_task:led_blink - Initial LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  // Verify first toggle to HIGH after blink interval
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS + 100)); // 1100 ms

  led_level = gpio_get_led_level(pin);
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);

  // Verify second toggle to LOW after blink interval
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS)); // 1000 ms

  led_level = gpio_get_led_level(pin);
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  // Verify third toggle to HIGH after blink interval
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS)); // 1000 ms

  led_level = gpio_get_led_level(pin);
  ESP_LOGI(TAG, "gpio_task:led_blink - LED Level after blink interval: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, led_level);
}

/** @brief Test: Initialize GPIO task
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, initialize_task)
{
  unsigned int led_level = gpio_get_led_level(LED_STATUS_PIN_1);
  ESP_LOGI(TAG, "gpio_task:initialize_task - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  led_level = gpio_get_led_level(LED_STATUS_PIN_2);
  ESP_LOGI(TAG, "gpio_task:initialize_task - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);

  led_level = gpio_get_led_level(HEART_LED_ARRAY_PIN);
  ESP_LOGI(TAG, "gpio_task:initialize_task - LED Level: %u", led_level);
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, led_level);
}

/** @brief Test: LED Solid State
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, led_solid)
{
  test_led_solid_state(LED_STATUS_PIN_1);
  test_led_solid_state(LED_STATUS_PIN_2);
  test_led_solid_state(HEART_LED_ARRAY_PIN);
}

/** @brief Test: LED Blink State
 *  @test Expected: GPIO task initializes successfully
 */
TEST(gpio_task, led_blink)
{
  test_led_blink_state(LED_STATUS_PIN_1);
  test_led_blink_state(LED_STATUS_PIN_2);
  test_led_blink_state(HEART_LED_ARRAY_PIN);
}

/** @brief Test: Blink Restart Deterministic
 *  @test Expected: Blinking restarts from LOW state after being stopped
 */
TEST(gpio_task, blink_restart_deterministic)
{
  // First blink cycle
  gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_BLINK);
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS + 100));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_2));

  // Stop blinking
  gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_OFF);
  vTaskDelay(pdMS_TO_TICKS(100));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_led_level(LED_STATUS_PIN_2));

  // Restart blink - should start from LOW again (not continue from where it stopped)
  gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_BLINK);
  vTaskDelay(pdMS_TO_TICKS(100));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_led_level(LED_STATUS_PIN_2));

  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_2));
}

/** @brief Test: Blink to Solid Stops Timer
 *  @test Expected: Switching from BLINK to SOLID stops the blink timer and sets LED HIGH
 */
TEST(gpio_task, blink_to_solid_stops_timer)
{
  gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_BLINK);
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS + 100));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_2));

  // Switch to solid - should force HIGH and stop toggling
  gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_SOLID);
  vTaskDelay(pdMS_TO_TICKS(100));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_2));

  // Wait multiple blink intervals - should remain HIGH
  vTaskDelay(pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS * 3));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_2));
}

/** @brief Test: Push Button ISR
 *  @test Expected: Button press updates LED state correctly
 */
TEST(gpio_task, push_button_isr)
{
  gpio_set_button_level(GPIO_LOW);
  gpio_simulate_button_press_isr();

  // The output LED is set to the button state
  // In this case, the button is set to LOW
  vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS + 100));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_led_level(HEART_LED_ARRAY_PIN));

  // Simulate a button press to set it HIGH
  gpio_set_button_level(GPIO_HIGH);
  gpio_simulate_button_press_isr();

  // In this case, the button is set to HIGH
  vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS + 100));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(HEART_LED_ARRAY_PIN));

  // Reset button state for test isolation
  gpio_set_button_level(GPIO_LOW);
  gpio_simulate_button_press_isr();
  vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS + 100));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_led_level(HEART_LED_ARRAY_PIN));
}

/** @brief Test: Rapid State Changes
 *  @test Expected: GPIO task handles rapid state changes without errors
 */
TEST(gpio_task, rapid_state_changes)
{
  for (int i = 0; i < 5; i++)
  {
    gpio_set_state(LED_STATUS_PIN_1, GPIO_STATE_LED_BLINK);
    gpio_set_state(LED_STATUS_PIN_1, GPIO_STATE_LED_SOLID);
    gpio_set_state(LED_STATUS_PIN_1, GPIO_STATE_LED_OFF);
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  TEST_ASSERT_EQUAL_UINT(GPIO_LOW, gpio_get_led_level(LED_STATUS_PIN_1));

  for (int i = 0; i < 5; i++)
  {
    gpio_set_state(LED_STATUS_PIN_1, GPIO_STATE_LED_BLINK);
    gpio_set_state(LED_STATUS_PIN_1, GPIO_STATE_LED_SOLID);
  }

  vTaskDelay(pdMS_TO_TICKS(100));
  TEST_ASSERT_EQUAL_UINT(GPIO_HIGH, gpio_get_led_level(LED_STATUS_PIN_1));
}

TEST_GROUP_RUNNER(gpio_task)
{
  RUN_TEST_CASE(gpio_task, initialize_task);
  RUN_TEST_CASE(gpio_task, led_solid);
  RUN_TEST_CASE(gpio_task, led_blink);
  RUN_TEST_CASE(gpio_task, blink_restart_deterministic);
  RUN_TEST_CASE(gpio_task, blink_to_solid_stops_timer);
  RUN_TEST_CASE(gpio_task, push_button_isr);
  RUN_TEST_CASE(gpio_task, rapid_state_changes);
}

