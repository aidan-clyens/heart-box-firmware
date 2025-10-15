#include "gpio_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#include "state_machine_task.h"

const char * TAG_BUTTON_TASK = "gpio_button_task";
const char * TAG_STATUS_TASK = "gpio_status_task";

static portMUX_TYPE gpio_mux = portMUX_INITIALIZER_UNLOCKED;

static TaskHandle_t gpio_status_task_handle = NULL;
static SemaphoreHandle_t gpio_button_semaphore = NULL;
static TimerHandle_t gpio_blink_timer = NULL;

volatile eGpioState_t gpio_current_state = GPIO_STATE_LED_OFF;

/** @brief Change the state of the GPIO status task
 *  @param state The state to change to
 */
void gpio_set_state(eGpioState_t state)
{
  taskENTER_CRITICAL(&gpio_mux);
  gpio_current_state = state;
  taskEXIT_CRITICAL(&gpio_mux);
  if (gpio_status_task_handle)
  {
    xTaskNotifyGive(gpio_status_task_handle);
  }
}

/** @brief Get the current state of the GPIO status task 
 *  @return eGpioState_t 
 */
static inline eGpioState_t gpio_get_state(void)
{
  eGpioState_t s;
  taskENTER_CRITICAL(&gpio_mux);
  s = gpio_current_state;
  taskEXIT_CRITICAL(&gpio_mux);
  return s;
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

/** @brief Initialize GPIO
 */
void gpio_initialize()
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
      .pull_up_en = GPIO_PULLUP_DISABLE,     // disable internal pull-up
      .pull_down_en = GPIO_PULLDOWN_DISABLE, // disable internal pull-down
      .intr_type = GPIO_INTR_ANYEDGE         // rising + falling edges
  };
  gpio_config(&button_pin_config);

  // Create binary semaphore
  gpio_button_semaphore = xSemaphoreCreateBinary();
  if (gpio_button_semaphore == NULL)
  {
    ESP_LOGE(TAG_BUTTON_TASK, "Failed to create Binary Semaphore");
    return;
  }

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
    if (xSemaphoreTake(gpio_button_semaphore, portMAX_DELAY))
    {
      gpio_intr_disable(BUTTON_PIN);

      // Button debouncing
      vTaskDelay(BUTTON_DEBOUNCE_TIME_MS / portTICK_PERIOD_MS);

      // Check if the button is pressed or released
      button_level = gpio_get_level(BUTTON_PIN);
      ESP_LOGI(TAG_BUTTON_TASK, "Button level: %d", button_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, button_level);

      // Send message to the state machine task
      state_machine_post_event(APP_EVENT_BUTTON_PRESSED);

      gpio_intr_enable(BUTTON_PIN);
    }
  }
}

/** @brief GPIO Status Task
 */
static void gpio_status_task(void *arg)
{
  gpio_status_task_handle = xTaskGetCurrentTaskHandle();
  ESP_LOGI(TAG_STATUS_TASK, "GPIO Status Task Started");

  while (true)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    eGpioState_t current_state = gpio_get_state();
    switch (current_state)
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
          gpio_blink_timer = xTimerCreate("blink", pdMS_TO_TICKS(1000), pdTRUE, NULL, gpio_blink_timer_cb);
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
  }
}

/** @brief Create GPIO Tasks
 */
void gpio_task_init()
{
  gpio_initialize();

  xTaskCreate(gpio_button_task, TAG_BUTTON_TASK, 2048, NULL, 10, NULL);
  xTaskCreate(gpio_status_task, TAG_STATUS_TASK, 2048, NULL, 10, NULL);
}
