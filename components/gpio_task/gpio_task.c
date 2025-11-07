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

/** @brief Post a command message to the GPIO task
 *  @param msg The command message to post
 */
BaseType_t gpio_post_msg(GpioMsg_t msg)
{
  return generic_task_post_msg(&gpio_task, &msg, sizeof(GpioMsg_t));
}

/** @brief Change the state of the GPIO status LED
 *  @param state The state to change to
 */
void gpio_set_state(eGpioState_t state)
{
  GpioMsg_t msg = {.type = APP_GPIO_CMD_SET_STATE, .data.state = state};
  gpio_post_msg(msg);
}

/** @brief Blink the status LED at a periodic interval
 *  @param xTimer
 */
static void gpio_blink_timer_cb(TimerHandle_t xTimer)
{
  static bool on = false;
  gpio_set_level(LED_STATUS_PIN_2, on);
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
}

/** @brief GPIO Button Task
 *
 *  Waits on the button semaphore, debounces, then posts a message
 *  to the GPIO task queue.
 */
static void gpio_button_task(void *args)
{
  ESP_LOGI(TAG_GPIO, "GPIO Button Task Started");

  int button_level = 0;

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

      vTaskDelay(BUTTON_DEBOUNCE_TIME_MS / portTICK_PERIOD_MS);

      button_level = gpio_get_level(BUTTON_PIN);
      ESP_LOGI(TAG_GPIO, "Button level: %d", button_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, button_level);

      // Notify state machine
      state_machine_post_event(APP_GPIO_EVT_BUTTON_PRESSED, APP_GPIO);

      gpio_intr_enable(BUTTON_PIN);
    }
#endif // DEBUG
  }
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
      if (gpio_blink_timer != NULL)
      {
        xTimerStop(gpio_blink_timer, 0);
      }
      gpio_set_level(LED_STATUS_PIN_2, 1);
      break;

    case GPIO_STATE_LED_BLINK:
      if (gpio_blink_timer == NULL)
      {
        gpio_blink_timer = xTimerCreate("blink",
                                        pdMS_TO_TICKS(1000),
                                        pdTRUE,
                                        NULL,
                                        gpio_blink_timer_cb);
      }
      xTimerStart(gpio_blink_timer, 0);
      break;

    case GPIO_STATE_LED_OFF:
    default:
      if (gpio_blink_timer != NULL)
      {
        xTimerStop(gpio_blink_timer, 0);
      }
      gpio_set_level(LED_STATUS_PIN_2, 0);
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
  gpio_task.on_init = NULL;
  gpio_task.on_message = gpio_on_message;
  gpio_task.item_size = sizeof(GpioMsg_t);
  generic_task_start(&gpio_task, 2048, 10);
}