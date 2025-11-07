#include "wifi_task.h"
#include "generic_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ping.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "ping/ping_sock.h"

#include "state_machine_task.h"

// --- Module State ---
static const char *TAG_WIFI = "WIFI";
static GenericTask wifi_task;

static bool is_wifi_started = false;
static bool is_wifi_connected = false;
static int wifi_connect_retries = 0;

/** @brief Post a command message to the WiFi task */
BaseType_t wifi_post_msg(WifiMsg_t msg)
{
  return generic_task_post_msg(&wifi_task, &msg, sizeof(WifiMsg_t));
}

/** @brief Public API: Set the WiFi to AP mode */
void wifi_set_ap_mode(void)
{
  WifiMsg_t msg = {.type = APP_WIFI_CMD_MODE_AP};
  wifi_post_msg(msg);
}

/** @brief Public API: Set the WiFi credentials to attempt to connect in STA mode
 *  @param ssid WiFi network name
 *  @param password WiFi network password
 */
void wifi_set_sta_credentials(const char *ssid, const char *password)
{
  WifiMsg_t msg = {.type = APP_WIFI_CMD_SET_STA_CREDENTIALS};
  strncpy(msg.data.credentials.ssid, ssid, MAX_SSID_LEN);
  msg.data.credentials.ssid[MAX_SSID_LEN] = '\0';
  strncpy(msg.data.credentials.password, password, MAX_PASSPHRASE_LEN);
  msg.data.credentials.password[MAX_PASSPHRASE_LEN] = '\0';
  wifi_post_msg(msg);
}

/** @brief Get the current WiFi credentials
 *  @return WiFi SSID and password
 */
WifiCredentials_t wifi_get_current_credentials(void)
{
  WifiCredentials_t creds = {};

  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);

  strncpy(creds.ssid, (char *)conf.sta.ssid, MAX_SSID_LEN);
  strncpy(creds.password, (char *)conf.sta.password, MAX_PASSPHRASE_LEN);

  return creds;
}

/** @brief Public API: Send a ping command to an external host
 *  @param hostname IPv4 or URL for external host
 */
void wifi_ping(const char *hostname)
{
  WifiMsg_t msg = {.type = APP_WIFI_CMD_PING};
  strncpy(msg.data.host, hostname, MAX_HOSTNAME_LEN);
  msg.data.host[MAX_HOSTNAME_LEN] = '\0';
  wifi_post_msg(msg);
}

/** @brief Event handler for WiFi and IP events
 *  @param arg
 *  @param event_base
 *  @param event_id
 *  @param event_data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      is_wifi_started = true;
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      is_wifi_connected = false;
      if (++wifi_connect_retries >= 5)
      {
        esp_wifi_stop();
        state_machine_post_event(APP_EVT_WIFI_DISCONNECTED, APP_WIFI);
        wifi_connect_retries = 0;
      }
      else
      {
        esp_wifi_connect();
      }
      break;

    case WIFI_EVENT_AP_START:
      is_wifi_started = true;
      state_machine_post_event(APP_EVT_AP_STARTED, APP_WIFI);
      break;

    case WIFI_EVENT_AP_STOP:
      is_wifi_started = false;
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    is_wifi_connected = true;
    wifi_connect_retries = 0;
    state_machine_post_event(APP_EVT_WIFI_CONNECTED, APP_WIFI);
  }
}

/** @brief On Ping command success callback function
 *  @param hdl Handle for the "ping" object
 *  @param args User arguments
 */
static void wifi_on_ping_success(esp_ping_handle_t hdl, void *args)
{
  state_machine_post_event(APP_EVT_PING_SUCCESS, APP_WIFI);
}

/** @brief On Ping command timeout callback function
 *  @param hdl Handle for the "ping" object
 *  @param args User arguments
 */
static void wifi_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
  state_machine_post_event(APP_EVT_PING_TIMEOUT, APP_WIFI);
}

/** @brief On Ping command end callback function
 *  @param hdl Handle for the "ping" object
 *  @param args User arguments
 */
static void wifi_on_ping_end(esp_ping_handle_t hdl, void *args)
{
  esp_ping_delete_session(hdl);
}

/** @brief Set WiFi configuration for STA mode */
static void wifi_set_config_sta(const char *ssid, const char *password)
{
  wifi_config_t wifi_config = {};
  strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

/** @brief Set WiFi configuration for AP mode */
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

/** @brief Send a ping command to an external host
 *  @param hostname IPv4 or URL for external host
 */
static void wifi_ping_start(const char *hostname)
{
  ip_addr_t target_addr;
  struct addrinfo hint, *res = NULL;
  memset(&hint, 0, sizeof(hint));
  int err = getaddrinfo(hostname, NULL, &hint, &res);
  if (err != 0 || res == NULL)
  {
    ESP_LOGE(TAG_WIFI, "DNS lookup failed for %s", hostname);
    return;
  }

  struct sockaddr_in *addr4 = (struct sockaddr_in *)res->ai_addr;
  inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4->sin_addr);
  freeaddrinfo(res);

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.target_addr = target_addr;
  ping_config.count = 1;
  esp_ping_callbacks_t cbs = {
      .on_ping_success = wifi_on_ping_success,
      .on_ping_timeout = wifi_on_ping_timeout,
      .on_ping_end = wifi_on_ping_end,
  };

  esp_ping_handle_t ping;
  esp_ping_new_session(&ping_config, &cbs, &ping);
  esp_ping_start(ping);

  ESP_LOGI(TAG_WIFI, "Pinging %s ...", hostname);
}

/** @brief Initialize WiFi
 *  @param self Pointer to the generic task object for the WiFi task
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
 *  @param self Pointer to the generic task object for the WiFi task
 *  @param msg_buf The message buffer queue for this task
 *  @param msg_len The length of a message for the message buffer
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
  case APP_WIFI_CMD_MODE_AP:
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_AP);
    wifi_set_config_ap("HeartBox", "password");
    esp_wifi_start();
    break;

  case APP_WIFI_CMD_SET_STA_CREDENTIALS:
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_set_config_sta(msg->data.credentials.ssid,
                        msg->data.credentials.password);
    esp_wifi_start();
    break;

  case APP_WIFI_CMD_PING:
    wifi_ping_start(msg->data.host);
    break;

  default:
    break;
  }
}

/** @brief Create WiFi Task */
void wifi_task_init(void)
{
  wifi_task.name = TAG_WIFI;
  wifi_task.on_init = wifi_on_init;
  wifi_task.on_message = wifi_on_message;
  wifi_task.item_size = sizeof(WifiMsg_t);
  generic_task_start(&wifi_task, 4096, 10);
}