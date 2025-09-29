#include "wifi_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_err.h"

#define WIFI_CONNECT_MAX_RETRIES 5

typedef enum
{
  WIFI_CMD_CONNECT = 0,
  WIFI_CMD_DISCONNECT,
  WIFI_CMD_MODE_STA,
  WIFI_CMD_MODE_AP,
} eWifiCommand_t;

static const char * TAG = "wifi_task";

const char * WIFI_SSID = "OctopusChurch";
const char * WIFI_PASSWORD = "BishopNemo";

static int wifi_connect_retries = 0;

static QueueHandle_t event_queue;

/** @brief Set WiFi to STA mode (Station Mode)
 */
void wifi_set_sta_mode()
{
  if (event_queue != NULL)
  {
    eWifiCommand_t cmd = WIFI_CMD_MODE_STA;
    xQueueSend(event_queue, &cmd, portMAX_DELAY);
  }
}

/** @brief Set WiFi to AP mode (Access Point Mode)
 */
void wifi_set_ap_mode()
{
  if (event_queue != NULL)
  {
    eWifiCommand_t cmd = WIFI_CMD_MODE_AP;
    xQueueSend(event_queue, &cmd, portMAX_DELAY);
  }
}

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
      ESP_LOGW(TAG, "Failed to connect to SSID:%s after %d attempts. Stopping...", WIFI_SSID, wifi_connect_retries);
      esp_wifi_stop();
    }

    esp_wifi_connect(); // retry
    wifi_connect_retries++;
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
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

/** @brief Set WiFi configuration for AP mode
 *  @param ssid     SSID of the AP
 *  @param password Password of the AP
 */
void wifi_set_config_ap(const char *ssid, const char *password)
{
  wifi_config_t ap_config = {};
  strlcpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
  strlcpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password));
  ap_config.ap.ssid_len = strlen(ssid);
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  if (strlen(password) == 0)
  {
    ap_config.ap.authmode = WIFI_AUTH_OPEN; // open AP if no password
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
}

/** @brief Initialize WiFi
 */
void wifi_initialize()
{
  event_queue = xQueueCreate(10, sizeof(eWifiCommand_t));
  if (event_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create event queue");
    return;
  }

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
  eWifiCommand_t cmd;
  ESP_LOGI(TAG, "WiFi Task Started");

  while (true)
  {
    if (xQueueReceive(event_queue, &cmd, portMAX_DELAY))
    {
      switch (cmd)
      {
      case WIFI_CMD_CONNECT:
        esp_wifi_connect();
        break;
      case WIFI_CMD_DISCONNECT:
        esp_wifi_disconnect();
        break;
      case WIFI_CMD_MODE_STA:
      {
        // Check if already in STA mode
        wifi_mode_t mode;
        ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
        if (mode == WIFI_MODE_STA)
        {
          ESP_LOGI(TAG, "Already in STA mode");
          break;
        }
        else if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA)
        {
          ESP_LOGI(TAG, "Switching from AP to STA mode");
          esp_wifi_stop();
        }
        else if (mode == WIFI_MODE_NULL)
        {
          // WiFi is not started
          ESP_LOGI(TAG, "WiFi is in NULL mode, starting STA mode");
        }

        wifi_set_config_sta(WIFI_SSID, WIFI_PASSWORD);
        ESP_ERROR_CHECK(esp_wifi_start());
        break;
      }
      case WIFI_CMD_MODE_AP:
        break;
      default:
        ESP_LOGW(TAG, "Unknown WiFi command");
        break;
      }
    }
  }
}

/** @brief Create WiFi Task
 */
void wifi_task_init()
{
  wifi_initialize();

  xTaskCreate(wifi_task, TAG, 2048, NULL, 10, NULL);
}