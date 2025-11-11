#include "gpio_task.h"
#include "generic_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "state_machine_task.h"

#define DEBUG

static const char *TAG_GPIO = "GPIO_TASK";

static GenericTask gpio_task;
static SemaphoreHandle_t gpio_button_semaphore = NULL;
static TimerHandle_t gpio_blink_timer = NULL;

static int gpio_status_led_current_level = GPIO_LOW;
static int gpio_output_led_current_level = GPIO_LOW;

#ifdef DEBUG_GPIO_BUTTON_ISR
static int gpio_simulated_button_level = GPIO_LOW;
#endif

/** @brief Post a command message to the GPIO task
 *  @param msg The command message to post
 */
BaseType_t gpio_post_msg(GpioMsg_t msg)
{
  return generic_task_post_msg(&gpio_task, &msg, sizeof(GpioMsg_t));
}

/** @brief Public API: Change the state of the GPIO status LED
 *  @param state The state to change to
 */
void gpio_set_state(eGpioState_t state)
{
  GpioMsg_t msg = {.type = APP_GPIO_CMD_SET_STATE, .data.state = state};
  gpio_post_msg(msg);
}

/** @brief Public API: Get the current level of the status LED
 *  @return The current level of the status LED (0 = OFF, 1 = ON)
 */
unsigned int gpio_get_status_led_level(void)
{
  return gpio_status_led_current_level;
}

/** @brief Public API: Get the current level of the output LED
 *  @return The current level of the output LED (0 = LOW, 1 = HIGH)
 */
unsigned int gpio_get_output_led_level(void)
{
  return gpio_output_led_current_level;
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
static void gpio_set_status_led_level(int level)
{
  gpio_status_led_current_level = level;
  ESP_LOGI(TAG_GPIO, "Setting Status LED level to %s", level == GPIO_HIGH ? "HIGH" : "LOW");
  gpio_set_level(LED_STATUS_PIN_2, gpio_status_led_current_level);
}

static int gpio_get_button_level()
{
#ifdef DEBUG_GPIO_BUTTON_ISR
  return gpio_simulated_button_level;
#else
  return gpio_get_level(BUTTON_PIN);
#endif
}

/** @brief Blink the status LED at a periodic interval
 *  @param xTimer
 */
static void gpio_blink_timer_cb(TimerHandle_t xTimer)
{
  static bool on = false;
  gpio_set_status_led_level(on ? GPIO_HIGH : GPIO_LOW);
  on = !on;
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

/** @brief Initialize GPIO hardware
 */
static void gpio_initialize(void)
{
  gpio_reset_pin(HEART_LED_ARRAY_PIN);
  gpio_set_direction(HEART_LED_ARRAY_PIN, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN_1);
  gpio_set_direction(LED_STATUS_PIN_1, GPIO_MODE_OUTPUT);

  gpio_reset_pin(LED_STATUS_PIN_2);
  gpio_set_direction(LED_STATUS_PIN_2, GPIO_MODE_OUTPUT);

  gpio_config_t button_pin_config = {
      .pin_bit_mask = (1ULL << BUTTON_PIN),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_ANYEDGE};
  gpio_config(&button_pin_config);

  gpio_button_semaphore = xSemaphoreCreateBinary();
  if (gpio_button_semaphore == NULL)
  {
    ESP_LOGE(TAG_GPIO, "Failed to create Binary Semaphore");
    return;
  }

  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)BUTTON_PIN);

  ESP_LOGI(TAG_GPIO, "GPIO Initialized");
}

/** @brief GPIO Button Task
 *
 *  Waits on the button semaphore, debounces, then posts a message
 *  to the GPIO task queue.
 */
static void gpio_button_task(void *args)
{
  ESP_LOGI(TAG_GPIO, "GPIO Button Task Started");

  while (true)
  {
#ifdef DEBUG
    // Simulate button press every 10 seconds
    vTaskDelay(pdMS_TO_TICKS(10000));

    state_machine_post_event(APP_GPIO_EVT_BUTTON_PRESSED, APP_GPIO);
    vTaskDelay(pdMS_TO_TICKS(1000));
    state_machine_post_event(APP_GPIO_EVT_BUTTON_RELEASED, APP_GPIO);
#else
    if (xSemaphoreTake(gpio_button_semaphore, portMAX_DELAY))
    {
      gpio_intr_disable(BUTTON_PIN);

      vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS));

      gpio_output_led_current_level = gpio_get_button_level();
      ESP_LOGI(TAG_GPIO, "Received Push Button Event - Button level: %d", gpio_output_led_current_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, gpio_output_led_current_level);

      // Notify state machine
      state_machine_post_event(APP_GPIO_EVT_BUTTON_PRESSED, APP_GPIO);

      gpio_intr_enable(BUTTON_PIN);
    }
#endif // DEBUG
  }
}

static void gpio_on_init(GenericTask *self)
{
  // No initialization needed
}

static void gpio_on_stop(GenericTask *self)
{
  if (gpio_blink_timer != NULL)
  {
    xTimerDelete(gpio_blink_timer, 0);
    gpio_blink_timer = NULL;
  }

  // Uninstall ISR service
  gpio_isr_handler_remove(BUTTON_PIN);
  gpio_uninstall_isr_service();

  gpio_set_status_led_level(GPIO_LOW);
}

/** @brief GPIO Task message handler
 *  @param self    Pointer to the GenericTask
 *  @param msg_buf Pointer to the received message buffer
 *  @param msg_len Length of the message buffer
 */
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
    switch (msg->data.state)
    {
    case GPIO_STATE_LED_SOLID:
      ESP_LOGI(TAG_GPIO, "Received message: GPIO_STATE_LED_SOLID");
      if (gpio_blink_timer != NULL)
      {
        xTimerStop(gpio_blink_timer, 0);
      }
      gpio_set_status_led_level(GPIO_HIGH);
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
      gpio_set_status_led_level(GPIO_LOW);
      break;
    }
    break;

  default:
    ESP_LOGW(TAG_GPIO, "Unknown GPIO message %d", msg->type);
    break;
  }
}

/** @brief Create GPIO Tasks
 */
void gpio_task_init(void)
{
  gpio_initialize();

  // Button task remains a raw FreeRTOS task
  xTaskCreate(gpio_button_task, TAG_GPIO, 2048, NULL, 15, NULL);

  // Status task uses GenericTask with queue
  gpio_task.name = TAG_GPIO;
  gpio_task.on_init = gpio_on_init;
  gpio_task.on_stop = gpio_on_stop;
  gpio_task.on_message = gpio_on_message;
  gpio_task.item_size = sizeof(GpioMsg_t);
  generic_task_start(&gpio_task, 2048, 10);
}

void gpio_task_stop(void)
{
  generic_task_stop(&gpio_task);
}

bool gpio_task_is_running(void)
{
  return generic_task_is_running(&gpio_task);
}
