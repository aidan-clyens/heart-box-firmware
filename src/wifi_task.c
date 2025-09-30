#include "wifi_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "ping/ping_sock.h"
#include "esp_wifi.h"
#include "esp_ping.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_err.h"

#include "state_machine_task.h"

#define WIFI_CONNECT_MAX_RETRIES 5

/** @enum WifiState_t
 *  @brief WiFi Application States
 */
typedef enum
{
  STATE_IDLE,
  STATE_PROVISIONING,
  STATE_CONNECTED,
} WifiState_t;

static const char * TAG = "wifi_task";

WifiState_t wifi_current_state = STATE_IDLE;

const char * WIFI_AP_SSID = "HeartBox";
const char * WIFI_AP_PASSWORD = "password";

const char * WIFI_SSID = "OctopusChurch";
const char * WIFI_PASSWORD = "BishopNemo";

static int wifi_connect_retries = 0;

static QueueHandle_t wifi_cmd_queue;

/** @brief Post a command to the WiFi task
 *  @param cmd The command to post
 */
void wifi_post_cmd(eWifiCommand_t cmd)
{
  if (wifi_cmd_queue != NULL)
  {
    xQueueSend(wifi_cmd_queue, &cmd, portMAX_DELAY);
  }
}

/** @brief Change the current WiFi state
 *  @param new_state The new state to change to
 */
void wifi_change_state(WifiState_t new_state)
{
  ESP_LOGI(TAG, "WiFi State changed from %d to %d", wifi_current_state, new_state);
  wifi_current_state = new_state;
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
  if (event_base == WIFI_EVENT)
  {
    if (event_id == WIFI_EVENT_STA_START)
    {
      esp_wifi_connect();
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
      if (wifi_connect_retries >= WIFI_CONNECT_MAX_RETRIES)
      {
        ESP_LOGW(TAG, "Failed to connect to SSID:%s after %d attempts. Stopping...", WIFI_SSID, wifi_connect_retries);
        esp_wifi_stop();
  
        wifi_change_state(STATE_IDLE);
      }
  
      esp_wifi_connect(); // retry
      wifi_connect_retries++;
    }
    else if (event_id == WIFI_EVENT_AP_START)
    {
      ESP_LOGI(TAG, "Access Point Started");
      // TODO - Start HTTP server

      wifi_change_state(STATE_PROVISIONING);
    }
    else if (event_id == WIFI_EVENT_AP_STOP)
    {
      ESP_LOGI(TAG, "Access Point Stopped");
      // TODO - Stop HTTP server

      wifi_change_state(STATE_IDLE);
    }
  }
  else if (event_base == IP_EVENT)
  {
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  
      state_machine_post_event(APP_EVENT_WIFI_CONNECTED);
      wifi_connect_retries = 0; // reset retry counter
  
      wifi_change_state(STATE_CONNECTED);
    }
  }
}

/** @brief On Ping Success Callback
 *  @param hdl  Ping handle
 *  @param args User defined argument 
 */
static void wifi_on_ping_success(esp_ping_handle_t hdl, void *args)
{
  ip_addr_t target_addr;
  uint32_t elapsed_time;
  uint16_t seqno;
  uint32_t recv_len;
  uint8_t ttl;

  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));

  ESP_LOGI(TAG, "%d bytes from %s icmp_seq=%d ttl=%d time=%d ms",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

/** @brief On Ping Timeout Callback
 *  @param hdl  Ping handle
 *  @param args User defined argument
 */
static void wifi_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
  ip_addr_t target_addr;
  uint16_t seqno;
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  ESP_LOGW(TAG, "From %s icmp_seq=%d timeout", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

/** @brief On Ping End Callback
 *  @param hdl  Ping handle
 *  @param args User defined argument
 */
static void wifi_on_ping_end(esp_ping_handle_t hdl, void *args)
{
  uint32_t transmitted, received, duration;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &duration, sizeof(duration));
  ESP_LOGI(TAG, "Ping finished: %d packets transmitted, %d received, time %dms",
           transmitted, received, duration);

  esp_ping_delete_session(hdl);
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

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
}

/** @brief Ping a host to check connectivity
 *  @param host The hostname or IP address to ping
 */
void wifi_ping(const char * host)
{
  if (host == NULL)
  {
    ESP_LOGE(TAG, "Host is NULL");
    return;
  }
  ESP_LOGI(TAG, "Pinging host: %s", host);

  // Resolve hostname to IP
  ip_addr_t target_addr;
  struct addrinfo hint, *res = NULL;
  memset(&hint, 0, sizeof(hint));
  int err = getaddrinfo(host, NULL, &hint, &res);
  if (err != 0 || res == NULL)
  {
    ESP_LOGE(TAG, "DNS lookup failed for %s", host);
    return;
  }
  struct sockaddr_in *addr4 = (struct sockaddr_in *)res->ai_addr;
  inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4->sin_addr);
  freeaddrinfo(res);

  // Configure ping
  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.target_addr = target_addr;
  ping_config.count = 4; // number of pings

  esp_ping_callbacks_t cbs = {
    .on_ping_success = wifi_on_ping_success,
    .on_ping_timeout = wifi_on_ping_timeout,
    .on_ping_end = wifi_on_ping_end,
  };

  esp_ping_handle_t ping;
  esp_ping_new_session(&ping_config, &cbs, &ping);
  esp_ping_start(ping);
}

/** @brief Initialize WiFi
 */
void wifi_initialize()
{
  // Initialize TCP/IP network interface and event loop
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

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
}

/** @brief WiFi Task
 */
static void wifi_task(void *args)
{
  eWifiCommand_t cmd;
  ESP_LOGI(TAG, "WiFi Task Started");

  while (true)
  {
    switch (wifi_current_state)
    {
    case STATE_IDLE:
      if (xQueueReceive(wifi_cmd_queue, &cmd, portMAX_DELAY))
      {
        if (cmd == WIFI_CMD_MODE_AP_STA)
        {
          ESP_LOGI(TAG, "Starting WiFi in AP+STA mode");
          wifi_set_config_ap(WIFI_AP_SSID, WIFI_AP_PASSWORD);
          ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
          ESP_ERROR_CHECK(esp_wifi_start());
        }
        else if (cmd == WIFI_CMD_SET_STA_CREDENTIALS)
        {
          ESP_LOGI(TAG, "Starting WiFi in STA mode");
          wifi_set_config_sta(WIFI_SSID, WIFI_PASSWORD);
          ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
          ESP_ERROR_CHECK(esp_wifi_start());
        }
      }
      break;
    case STATE_PROVISIONING:
      break;
    case STATE_CONNECTED:
      if (xQueueReceive(wifi_cmd_queue, &cmd, portMAX_DELAY))
      {
        if (cmd == WIFI_CMD_PING)
        {
          wifi_ping("www.google.com");
        }
      }
      break;
    default:
      break;
    }
  }
}

/** @brief Create WiFi Task
 */
void wifi_task_init()
{
  wifi_cmd_queue = xQueueCreate(10, sizeof(eWifiCommand_t));
  if (wifi_cmd_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create command queue");
    return;
  }

  wifi_initialize();

  xTaskCreate(wifi_task, TAG, 4096, NULL, 10, NULL);
}