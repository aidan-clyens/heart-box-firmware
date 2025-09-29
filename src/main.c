#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include <stdio.h>

#define HEART_LED_ARRAY_PIN GPIO_NUM_14
#define LED_STATUS_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

static SemaphoreHandle_t button_semaphore = NULL;

/** ISRs **/

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

/** Event Handlers **/

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    esp_wifi_connect(); // retry
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI("wifi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

/** TASKS **/

/** @brief Push Button Handler Task
 */
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
      ESP_LOGI("gpio", "Button level: %d\n", button_level);
      gpio_set_level(HEART_LED_ARRAY_PIN, button_level);

      gpio_intr_enable(BUTTON_PIN);
    }
  }
}

/** @brief LED Blink Task
 */
static void led_blink_task(void *arg)
{
  while (true)
  {
    ESP_LOGI("gpio", "Turning LED ON\n");
    gpio_set_level(LED_STATUS_PIN, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI("gpio", "Turning LED OFF\n");
    gpio_set_level(LED_STATUS_PIN, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/** INITIALIZATION FUNCTIONS **/

/** @brief Initialize GPIO
 */
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

/** @brief Initialize Non-Volatile Storage (NVS)
 */
void initialize_nvs()
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

/** @brief Initialize WiFi
 */
void initialize_wifi()
{
  // Initialize TCP/IP network interface and event loop
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();

  // Create event handler for WiFi and IP events
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  // Configure Wifi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = "OctopusChurch",
      .password = "BishopNemo",
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

/** MAIN **/

/** @brief Main Task
 */
void app_main()
{
  initialize_gpio();
  initialize_nvs();
  initialize_wifi();

  // Create binary semaphore
  button_semaphore = xSemaphoreCreateBinary();

  // Create GPIO tasks
  xTaskCreate(push_button_handler_task, "push_button_handler_task", 2048, NULL, 10, NULL);
  xTaskCreate(led_blink_task, "led_blink_task", 2048, NULL, 10, NULL);

  // Install ISR for button pin
  gpio_install_isr_service(0);
  gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*)BUTTON_PIN);
}