#include "gpio_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "state_machine_task.h"

#define NUM_GPIO_PINS 40 // How many GPIO pins are available on the ESP32?

static const char *TAG_GPIO = "GPIO_TASK";
static const int BUTTON_PRESS_RESET_TIME_MS = 20000; // 20 seconds to trigger factory reset

// Forward declarations
static esp_err_t gpio_on_init(GenericTask *self);
static esp_err_t gpio_on_stop(GenericTask *self);
static void gpio_on_message(GenericTask *self, void *msg_buf, size_t msg_len);
static void gpio_button_task(void *args);

static GenericTask *gpio_task;
static TaskHandle_t gpio_button_task_handle = NULL;
static SemaphoreHandle_t gpio_button_semaphore = NULL;
static TimerHandle_t gpio_blink_timer = NULL;
static TimerHandle_t gpio_led_timer = NULL;

static int gpio_button_pressed_time_ms = 0;

static int gpio_led_current_state[NUM_GPIO_PINS] = {0};

static int gpio_active_led_pin = 0;

#ifdef DEBUG_GPIO_BUTTON_ISR
static int gpio_simulated_button_level = GPIO_LOW;
#endif

/** @brief Post a command message to the GPIO task
 *  @param msg The command message to post
 */
BaseType_t gpio_post_msg(GpioMsg_t msg)
{
  return generic_task_post_msg(gpio_task, &msg, sizeof(GpioMsg_t));
}

/** @brief Public API: Change the state of a GPIO LED
 *  @param state The state to change to
 */
void gpio_set_state(gpio_num_t pin, eGpioState_t state)
{
  GpioLedStateMsg_t led_state_msg = {.pin = pin, .state = state, .duration_ms = 0};
  GpioMsg_t msg = {.type = APP_GPIO_CMD_SET_STATE, .data.led_state = led_state_msg};
  gpio_post_msg(msg);
}

/** @brief Public API: Change the state of a GPIO LED for a specified duration
 *  @param state The state to change to
 *  @param duration_ms Duration in milliseconds to maintain the state
 */
void gpio_set_state_with_duration(gpio_num_t pin, eGpioState_t state, unsigned int duration_ms)
{
  GpioLedStateMsg_t led_state_msg = {.pin = pin, .state = state, .duration_ms = duration_ms};
  GpioMsg_t msg = {.type = APP_GPIO_CMD_SET_STATE, .data.led_state = led_state_msg};
  gpio_post_msg(msg);
}

/** @brief Public API: Get the current level of the status LED pin
 *  @return The current level of the status LED (0 = OFF, 1 = ON)
 */
unsigned int gpio_get_led_level(gpio_num_t pin)
{
  if (pin >= NUM_GPIO_PINS)
  {
    ESP_LOGE(TAG_GPIO, "Invalid GPIO pin number: %d", pin);
    return GPIO_LOW;
  }

  return gpio_led_current_state[pin];
}

#ifdef DEBUG_GPIO_BUTTON_ISR
void gpio_set_button_level(int level)
{
  gpio_simulated_button_level = level;
}
#endif

/** @brief Set the status LED to ON or OFF
 *  @param level The level to set (0 = OFF, 1 = ON)
 */
static void gpio_set_led_level(gpio_num_t pin, int level)
{
  if (pin >= NUM_GPIO_PINS)
  {
    ESP_LOGE(TAG_GPIO, "Invalid GPIO pin number: %d", pin);
    return;
  }

  gpio_led_current_state[pin] = level;
  // Reduced to DEBUG level to save stack space in timer callbacks
  ESP_LOGD(TAG_GPIO, "Setting LED pin %d level to %s", pin, level == GPIO_HIGH ? "HIGH" : "LOW");
  gpio_set_level(pin, gpio_led_current_state[pin]);
}

static int gpio_get_button_level()
{
#ifdef DEBUG_GPIO_BUTTON_ISR
  return gpio_simulated_button_level;
#else
  return gpio_get_level(BUTTON_PIN);
#endif
}

/** @brief Publish a button event to the State Machine
 *  @param duration_ms Duration the button was pressed in milliseconds
 */
static void gpio_publish_button_event(unsigned int duration_ms)
{
  // Notify state machine
  GpioMsg_t gpio_msg = {
    .type = APP_GPIO_EVT_BUTTON_PRESSED,
    .data.button_event = { .duration_ms = duration_ms }
  };

  AppMsg_t app_msg = {
    .type = APP_GPIO_EVT_BUTTON_PRESSED,
    .data = { .gpio = gpio_msg },
    .source = APP_GPIO
  };

  state_machine_post_event_msg(&app_msg);
}

/** @brief Blink the status LED at a periodic interval
 *  @param xTimer
 */
static void gpio_blink_timer_cb(TimerHandle_t xTimer)
{
  unsigned int current_level = gpio_get_led_level(LED_STATUS_PIN_2);
  gpio_set_led_level(LED_STATUS_PIN_2, !current_level);
}

/** @brief Turn off the LED after the specified duration
 */
static void gpio_led_timer_cb(TimerHandle_t xTimer)
{
  if (gpio_active_led_pin != 0)
  {
    gpio_set_led_level(gpio_active_led_pin, GPIO_LOW);
    gpio_active_led_pin = 0;
  }
}

/** @brief Push Button Interrupt Service Routine
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(gpio_button_semaphore, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

#ifdef DEBUG_GPIO_BUTTON_ISR
/** @brief Public API: Simulate a button press ISR for testing purposes */
void gpio_simulate_button_press_isr(void)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(gpio_button_semaphore, &xHigherPriorityTaskWoken);
}
#endif

/** @brief GPIO Button Task
 *
 *  Waits on the button semaphore, debounces, then posts a message
 *  to the state machine.
 */
static void gpio_button_task(void *args)
{
  ESP_LOGI(TAG_GPIO, "GPIO Button Task Started");

  while (true)
  {
    if (xSemaphoreTake(gpio_button_semaphore, portMAX_DELAY))
    {
      gpio_intr_disable(BUTTON_PIN);

      vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS));

      // Clear any pending events that arrived during debounce
      while (xSemaphoreTake(gpio_button_semaphore, 0) == pdTRUE)
      {
        // Drain the semaphore
      }

      int button_level = gpio_get_button_level();
      gpio_led_current_state[HEART_LED_ARRAY_PIN] = button_level;
      ESP_LOGI(TAG_GPIO, "Received Push Button Event - Button level: %d", button_level);
      gpio_set_led_level(HEART_LED_ARRAY_PIN, button_level);

      // If button was pressed, get the current time
      if (button_level == GPIO_HIGH)
      {
        gpio_button_pressed_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      }
      else
      {
        if (gpio_button_pressed_time_ms != 0)
        {
          int press_duration_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - gpio_button_pressed_time_ms;
          ESP_LOGI(TAG_GPIO, "Button was pressed for %u ms", press_duration_ms);
          gpio_button_pressed_time_ms = 0;

          if (press_duration_ms >= BUTTON_PRESS_RESET_TIME_MS)
          {
            ESP_LOGW(TAG_GPIO, "Button press duration exceeded %d ms, triggering factory reset", BUTTON_PRESS_RESET_TIME_MS);
            // Notify state machine to perform factory reset
            state_machine_post_event(APP_CMD_FACTORY_RESET, APP_GPIO);
          }
          else
          {
            // Notify state machine
            gpio_publish_button_event(press_duration_ms);
          }
        }
      }

      gpio_intr_enable(BUTTON_PIN);
    }
  }
}

static esp_err_t gpio_on_init(GenericTask *self)
{
  esp_err_t ret;

  memset(gpio_led_current_state, 0, sizeof(gpio_led_current_state));

  // Step 1: Initialize GPIO pins
  gpio_reset_pin(HEART_LED_ARRAY_PIN);
  gpio_set_direction(HEART_LED_ARRAY_PIN, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN_1);
  gpio_set_direction(LED_STATUS_PIN_1, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN_2);
  gpio_set_direction(LED_STATUS_PIN_2, GPIO_MODE_OUTPUT);

  gpio_set_led_level(HEART_LED_ARRAY_PIN, GPIO_LOW);
  gpio_set_led_level(LED_STATUS_PIN_1, GPIO_LOW);
  gpio_set_led_level(LED_STATUS_PIN_2, GPIO_LOW);

  gpio_config_t button_pin_config = {
      .pin_bit_mask = (1ULL << BUTTON_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE};
  
  ret = gpio_config(&button_pin_config);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to configure button pin");
    return ret;
  }

  // Step 2: Create button semaphore
  gpio_button_semaphore = xSemaphoreCreateBinary();
  if (gpio_button_semaphore == NULL)
  {
    ESP_LOGE(TAG_GPIO, "Failed to create button semaphore");
    return ESP_ERR_NO_MEM;
  }

  // Step 3: Install ISR service and handler
  ret = gpio_install_isr_service(0);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to install ISR service");
    goto cleanup_semaphore;
  }

  ret = gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)BUTTON_PIN);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to add ISR handler");
    goto cleanup_isr_service;
  }

  // Step 4: Create button task
  BaseType_t task_created = xTaskCreate(
      gpio_button_task, 
      "GPIO_BTN", 
      2048, 
      NULL, 
      15, 
      &gpio_button_task_handle);
  
  if (task_created != pdPASS || gpio_button_task_handle == NULL)
  {
    ESP_LOGE(TAG_GPIO, "Failed to create button task");
    ret = ESP_ERR_NO_MEM;
    goto cleanup_isr_handler;
  }

  ESP_LOGI(TAG_GPIO, "GPIO Task Initialized");
  return ESP_OK;

  // Cleanup path on error (reverse order)
cleanup_isr_handler:
  gpio_isr_handler_remove(BUTTON_PIN);
cleanup_isr_service:
  gpio_uninstall_isr_service();
cleanup_semaphore:
  vSemaphoreDelete(gpio_button_semaphore);
  gpio_button_semaphore = NULL;
  return ret;
}

static esp_err_t gpio_on_stop(GenericTask *self)
{
  esp_err_t ret;

  // Step 1: Stop and delete blink timer
  if (gpio_blink_timer != NULL)
  {
    xTimerStop(gpio_blink_timer, portMAX_DELAY);
    xTimerDelete(gpio_blink_timer, portMAX_DELAY);
    gpio_blink_timer = NULL;
  }

  // Step 1a: Stop and delete LED timer
  if (gpio_led_timer != NULL)
  {
    xTimerStop(gpio_led_timer, portMAX_DELAY);
    xTimerDelete(gpio_led_timer, portMAX_DELAY);
    gpio_led_timer = NULL;
  }

  // Step 2: Stop button task
  if (gpio_button_task_handle != NULL)
  {
    vTaskDelete(gpio_button_task_handle);
    gpio_button_task_handle = NULL;
  }

  // Step 3: Remove ISR handler
  ret = gpio_isr_handler_remove(BUTTON_PIN);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to remove ISR handler for button pin: %s", esp_err_to_name(ret));
    // Continue cleanup even if this fails
  }

  // Step 4: Uninstall ISR service
  gpio_uninstall_isr_service();

  // Step 5: Delete button semaphore
  if (gpio_button_semaphore != NULL)
  {
    vSemaphoreDelete(gpio_button_semaphore);
    gpio_button_semaphore = NULL;
  }

  // Step 6: Reset GPIO pins to safe state
  gpio_set_led_level(HEART_LED_ARRAY_PIN, GPIO_LOW);
  gpio_set_led_level(LED_STATUS_PIN_1, GPIO_LOW);
  gpio_set_led_level(LED_STATUS_PIN_2, GPIO_LOW);

  gpio_reset_pin(HEART_LED_ARRAY_PIN);
  gpio_reset_pin(LED_STATUS_PIN_1);
  gpio_reset_pin(LED_STATUS_PIN_2);
  gpio_reset_pin(BUTTON_PIN);

  ESP_LOGI(TAG_GPIO, "GPIO Task Stopped");
  return ESP_OK;
}

static void gpio_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  if (msg_len != sizeof(GpioMsg_t))
  {
    return;
  }

  GpioMsg_t *msg = (GpioMsg_t *)msg_buf;

  switch (msg->type)
  {
  case APP_GPIO_CMD_SET_STATE:
    switch (msg->data.led_state.state)
    {
    case GPIO_STATE_LED_SOLID:
      ESP_LOGI(TAG_GPIO, "Received message: GPIO_STATE_LED_SOLID");
      if (gpio_blink_timer != NULL)
      {
        xTimerStop(gpio_blink_timer, 0);
      }
      gpio_set_led_level(msg->data.led_state.pin, GPIO_HIGH);

      // If a duration is specified, set a timer to turn off the LED after the duration
      if (msg->data.led_state.duration_ms > 0)
      {
        // Delete existing timer if it exists since duration may be different
        if (gpio_led_timer != NULL)
        {
          xTimerStop(gpio_led_timer, 0);
          xTimerDelete(gpio_led_timer, 0);
          gpio_led_timer = NULL;
        }

        // Create one-shot timer with the new duration
        gpio_led_timer = xTimerCreate("led_timer",
          pdMS_TO_TICKS(msg->data.led_state.duration_ms),
          pdFALSE,  // One-shot timer
          NULL,
          gpio_led_timer_cb);
        
        if (gpio_led_timer != NULL)
        {
          gpio_active_led_pin = msg->data.led_state.pin;
          xTimerStart(gpio_led_timer, 0);
        }
        else
        {
          ESP_LOGE(TAG_GPIO, "Failed to create LED timer");
        }
      }
      break;

    case GPIO_STATE_LED_BLINK:
      ESP_LOGI(TAG_GPIO, "Received message: GPIO_STATE_LED_BLINK");
      if (gpio_blink_timer == NULL)
      {
        gpio_blink_timer = xTimerCreate("blink",
                                        pdMS_TO_TICKS(GPIO_LED_BLINK_INTERVAL_MS),
                                        pdTRUE,
                                        NULL,
                                        gpio_blink_timer_cb);
      }
      xTimerStart(gpio_blink_timer, 0);
      break;

    case GPIO_STATE_LED_OFF:
      ESP_LOGI(TAG_GPIO, "Received message: GPIO_STATE_LED_OFF");
    default:
      if (gpio_blink_timer != NULL)
      {
        xTimerStop(gpio_blink_timer, 0);
      }
      gpio_set_led_level(msg->data.led_state.pin, GPIO_LOW);
      break;
    }
    break;

  default:
    ESP_LOGW(TAG_GPIO, "Unknown GPIO message %d", msg->type);
    break;
  }
}

/** @brief Initialize and start GPIO Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t gpio_task_init(void)
{
  ESP_LOGI(TAG_GPIO, "Initializing GPIO Task...");

  // Create GenericTask instance
  gpio_task = generic_task_create(
      TAG_GPIO,
      sizeof(GpioMsg_t),
      gpio_on_init,
      gpio_on_message,
      gpio_on_stop);

  if (gpio_task == NULL)
  {
    ESP_LOGE(TAG_GPIO, "Failed to create GPIO GenericTask");
    return ESP_ERR_NO_MEM;
  }

  // Start the task with increased stack size to accommodate logging overhead
  // 2048 was causing stack overflow with extensive ESP_LOGI calls
  esp_err_t ret = generic_task_start(gpio_task, 4096, 10);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to start GPIO GenericTask: %s", esp_err_to_name(ret));
    generic_task_delete(gpio_task);
    gpio_task = NULL;
    return ret;
  }

  ESP_LOGI(TAG_GPIO, "GPIO Task started successfully");
  return ESP_OK;
}

/** @brief Stop and clean up GPIO Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t gpio_task_deinit(void)
{
  if (gpio_task == NULL)
  {
    ESP_LOGW(TAG_GPIO, "GPIO Task not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG_GPIO, "Stopping GPIO Task...");

  // Stop the task
  esp_err_t ret = generic_task_stop(gpio_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to stop GPIO Task: %s", esp_err_to_name(ret));
    return ret;
  }

  // Delete the task
  ret = generic_task_delete(gpio_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GPIO, "Failed to delete GPIO Task: %s", esp_err_to_name(ret));
    return ret;
  }

  gpio_task = NULL;
  ESP_LOGI(TAG_GPIO, "GPIO Task stopped and cleaned up");
  return ESP_OK;
}
