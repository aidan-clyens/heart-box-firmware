#include "wifi_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_err.h"

#define WIFI_CONNECT_MAX_RETRIES 5

static const char * TAG_BUTTON_TASK = "wifi_task";

const char * WIFI_SSID = "OctopusChurch";
const char * WIFI_PASSWORD = "BishopNemo";

static int wifi_connect_retries = 0;

/** Event Handlers **/

/** @brief Event handler for WiFi and IP events
 *  @param arg         User defined argument
 *  @param event_base  Event base
 *  @param event_id    Event ID
 *  @param event_data  Event data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (wifi_connect_retries >= WIFI_CONNECT_MAX_RETRIES)
    {
      ESP_LOGW(TAG_BUTTON_TASK, "Failed to connect to SSID:%s after %d attempts. Stopping...", WIFI_SSID, wifi_connect_retries);
      esp_wifi_stop();
    }

    esp_wifi_connect(); // retry
    wifi_connect_retries++;
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG_BUTTON_TASK, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

/** @brief Set WiFi configuration for STA mode
 *  @param ssid     SSID of the target AP
 *  @param password Password of the target AP
 */
void wifi_set_config_sta(const char* ssid, const char* password)
{
  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

/** @brief Initialize WiFi
 */
void wifi_initialize()
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

  wifi_set_config_sta(WIFI_SSID, WIFI_PASSWORD);

  ESP_ERROR_CHECK(esp_wifi_start());
}

/** @brief WiFi Task
 */
static void wifi_task(void *args)
{
  ESP_LOGI(TAG_BUTTON_TASK, "WiFi Task Started");

  while (true)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/** @brief Create WiFi Task
 */
void wifi_task_init()
{
  wifi_initialize();

  xTaskCreate(wifi_task, TAG_BUTTON_TASK, 2048, NULL, 10, NULL);
}