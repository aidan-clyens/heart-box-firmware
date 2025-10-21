#include "wifi_task.h"
#include "generic_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/netdb.h"

#include "state_machine_task.h"

// --- Module State ---
static const char *TAG_WIFI = "WIFI";
static GenericTask wifi_task;

static bool is_wifi_started = false;
static bool is_wifi_connected = false;
static int wifi_connect_retries = 0;

/** @brief Post a command message to the WiFi task
 *  @param msg The command message to post
 */
BaseType_t wifi_post_msg(WifiMsg_t msg)
{
  return generic_task_post_msg(&wifi_task, &msg, sizeof(WifiMsg_t));
}

/** @brief Set the WiFi to AP mode
 */
void wifi_set_ap_mode(void)
{
  WifiMsg_t msg = {.type = WIFI_CMD_MODE_AP};
  wifi_post_msg(msg);
}

/** @brief Set the WiFi credentials to attempt to connect in STA mode
 *
 *  @param ssid WiFi SSID
 *  @param password WiFi password
 */
void wifi_set_sta_credentials(const char *ssid, const char *password)
{
  WifiMsg_t msg = {.type = WIFI_CMD_SET_STA_CREDENTIALS};
  strncpy(msg.data.credentials.ssid, ssid, MAX_SSID_LEN);
  msg.data.credentials.ssid[MAX_SSID_LEN] = '\0';
  strncpy(msg.data.credentials.password, password, MAX_PASSPHRASE_LEN);
  msg.data.credentials.password[MAX_PASSPHRASE_LEN] = '\0';
  wifi_post_msg(msg);
}

/** @brief Event handler for WiFi and IP events
 *  @param arg         User defined argument
 *  @param event_base  Event base
 *  @param event_id    Event ID
 *  @param event_data  Event data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  WifiMsg_t msg = {.type = WIFI_MSG_NONE};

  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      msg.type = WIFI_EVT_STA_START;
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      msg.type = WIFI_EVT_STA_DISCONNECTED;
      break;
    case WIFI_EVENT_AP_START:
      msg.type = WIFI_EVT_AP_START;
      break;
    case WIFI_EVENT_AP_STOP:
      msg.type = WIFI_EVT_AP_STOP;
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    msg.type = WIFI_EVT_STA_GOT_IP;
  }

  if (msg.type != WIFI_MSG_NONE)
  {
    wifi_post_msg(msg);
  }
}

/** @brief Set WiFi configuration for STA mode
 *  @param ssid     SSID of the target AP
 *  @param password Password of the target AP
 */
static void wifi_set_config_sta(const char *ssid, const char *password)
{
  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

/** @brief Set WiFi configuration for AP mode
 *  @param ssid     SSID of the AP
 *  @param password Password of the AP
 */
static void wifi_set_config_ap(const char *ssid, const char *password)
{
  wifi_config_t ap_config = {};
  strlcpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
  strlcpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password));
  ap_config.ap.ssid_len = strlen(ssid);
  ap_config.ap.max_connection = 4;
  ap_config.ap.authmode = (strlen(password) == 0) ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
}

/** @brief Initialize WiFi
 */
static void wifi_on_init(GenericTask *self)
{
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifi_event_handler, NULL, &instance_any_id);

  esp_event_handler_instance_t instance_got_ip;
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &wifi_event_handler, NULL, &instance_got_ip);
}

/** @brief WiFi Task message handler
 *  @param self Pointer to the GenericTask
 *  @param msg_buf Pointer to the received message buffer
 *  @param msg_len Length of the message buffer
 */
static void wifi_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  if (msg_len != sizeof(WifiMsg_t))
  {
    return;
  }

  WifiMsg_t *msg = (WifiMsg_t *)msg_buf;

  switch (msg->type)
  {
  case WIFI_CMD_MODE_AP:
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_AP);
    wifi_set_config_ap("HeartBox", "password");
    esp_wifi_start();
    break;

  case WIFI_CMD_MODE_AP_STA:
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_set_config_ap("HeartBox", "password");
    wifi_set_config_sta("OctopusChurch", "BishopNemo");
    esp_wifi_start();
    break;

  case WIFI_CMD_SET_STA_CREDENTIALS:
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_set_config_sta(msg->data.credentials.ssid,
                        msg->data.credentials.password);
    esp_wifi_start();
    break;

  /** @brief Ping a host to check connectivity
   *  @param host The hostname or IP address to ping
   */
  case WIFI_CMD_PING:
    ESP_LOGI(TAG_WIFI, "Would ping host: %s", msg->data.host);
    break;

  case WIFI_EVT_STA_START:
    is_wifi_started = true;
    esp_wifi_connect();
    break;

  case WIFI_EVT_STA_DISCONNECTED:
    is_wifi_connected = false;
    if (++wifi_connect_retries >= 5)
    {
      esp_wifi_stop();
      state_machine_post_event(APP_EVENT_WIFI_DISCONNECTED);
      wifi_connect_retries = 0;
    }
    else
    {
      esp_wifi_connect();
    }
    break;

  case WIFI_EVT_AP_START:
    is_wifi_started = true;
    state_machine_post_event(APP_EVENT_AP_STARTED);
    break;

  case WIFI_EVT_AP_STOP:
    is_wifi_started = false;
    break;

  case WIFI_EVT_STA_GOT_IP:
    is_wifi_connected = true;
    wifi_connect_retries = 0;
    state_machine_post_event(APP_EVENT_WIFI_CONNECTED);
    break;

  default:
    break;
  }
}

/** @brief Create WiFi Task
 */
void wifi_task_init(void)
{
  wifi_task.name = TAG_WIFI;
  wifi_task.on_init = wifi_on_init;
  wifi_task.on_message = wifi_on_message;
  wifi_task.item_size = sizeof(WifiMsg_t);
  generic_task_start(&wifi_task, 4096, 10);
}