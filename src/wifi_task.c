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
#define WIFI_PING_HOSTNAME_LEN   64

/** @struct WifiPingRequest_t
 *  @brief Structure containing the hostname for a ping request
 */
typedef struct
{
  char host[WIFI_PING_HOSTNAME_LEN];
} WifiPingRequest_t;

static const char * TAG_WIFI = "WIFI";
static const char * TAG_PING = "WIFI_PING";

const char * WIFI_AP_SSID = "HeartBox";
const char * WIFI_AP_PASSWORD = "password";

const char * WIFI_SSID = "OctopusChurch";
const char * WIFI_PASSWORD = "BishopNemo";

static bool is_wifi_started = false;
static bool is_wifi_connected = false;

static int wifi_connect_retries = 0;

static QueueHandle_t wifi_msg_queue;
static QueueHandle_t wifi_ping_queue;

/** @brief Post a command message to the WiFi task
 *  @param msg The command message to post
 */
BaseType_t wifi_post_msg(WifiMsg_t msg)
{
  if (wifi_msg_queue != NULL)
  {
    return xQueueSend(wifi_msg_queue, &msg, pdMS_TO_TICKS(50));
  }

  ESP_LOGW(TAG_WIFI, "Failed to post WiFi message. Message queue is full");
  return errQUEUE_FULL;
}

/** @brief Set the WiFi to AP mode 
 */
void wifi_set_ap_mode()
{
  WifiMsg_t msg = {0};
  msg.type = WIFI_CMD_MODE_AP;

  wifi_post_msg(msg);
}

/** @brief Set the WiFi credentials to attempt to connect in STA mode
 * 
 *  @param ssid WiFi SSID
 *  @param password WiFi password
 */
void wifi_set_sta_credentials(const char *ssid, const char *password)
{
  WifiMsg_t msg = {0};
  msg.type = WIFI_CMD_SET_STA_CREDENTIALS;

  strncpy(msg.data.credentials.ssid, ssid, MAX_SSID_LEN);
  msg.data.credentials.ssid[sizeof(msg.data.credentials.ssid) - 1] = '\0';

  strncpy(msg.data.credentials.password, password, MAX_PASSPHRASE_LEN);
  msg.data.credentials.password[sizeof(msg.data.credentials.password) - 1] = '\0';

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
  WifiMsg_t msg = {0};
  msg.type = WIFI_MSG_NONE;

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

  ESP_LOGI(TAG_WIFI, "%d bytes from %s icmp_seq=%d ttl=%d time=%d ms",
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
  ESP_LOGW(TAG_WIFI, "From %s icmp_seq=%d timeout", inet_ntoa(target_addr.u_addr.ip4), seqno);
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
  ESP_LOGI(TAG_WIFI, "Ping finished: %d packets transmitted, %d received, time %dms",
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

  ESP_LOGI(TAG_WIFI, "Setting WiFi SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
  if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to set WiFi config. STA SSID:%s and password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
  }
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

  ESP_LOGI(TAG_WIFI, "Setting AP SSID:%s password:%s", ap_config.ap.ssid, ap_config.ap.password);
  if (esp_wifi_set_config(WIFI_IF_AP, &ap_config) != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to set WiFi config. AP SSID:%s and password:%s", ap_config.ap.ssid, ap_config.ap.password);
  }
}

/** @brief Ping a host to check connectivity
 *  @param host The hostname or IP address to ping
 */
void wifi_ping(const char * host)
{
  if (host == NULL)
  {
    ESP_LOGE(TAG_WIFI, "Host is NULL");
    return;
  }
  ESP_LOGI(TAG_WIFI, "Pinging host: %s", host);

  // Resolve hostname to IP
  ip_addr_t target_addr;
  struct addrinfo hint, *res = NULL;
  memset(&hint, 0, sizeof(hint));
  int err = getaddrinfo(host, NULL, &hint, &res);
  if (err != 0 || res == NULL)
  {
    ESP_LOGE(TAG_WIFI, "DNS lookup failed for %s", host);
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
  if (esp_netif_init() != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to initialize TCP/IP network interface");
    return;
  }

  if (esp_event_loop_create_default() != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to initialize TCP/IP event loop");
    return;
  }

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  // Create event handler for WiFi and IP events
  esp_event_handler_instance_t instance_any_id;
  if (esp_event_handler_instance_register(WIFI_EVENT,
                                          ESP_EVENT_ANY_ID,
                                          &wifi_event_handler,
                                          NULL,
                                          &instance_any_id) != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create event handler for WiFi events");
    return;
  }

  esp_event_handler_instance_t instance_got_ip;
  if (esp_event_handler_instance_register(IP_EVENT,
                                          IP_EVENT_STA_GOT_IP,
                                          &wifi_event_handler,
                                          NULL,
                                          &instance_got_ip) != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create event handler for IP events");
    return;
  }

  // Configure Wifi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed initialize WiFi");
    return;
  }
}

/** @brief WiFi Task
 */
static void wifi_task(void *args)
{
  WifiMsg_t msg;
  ESP_LOGI(TAG_WIFI, "WiFi Task Started");

  while (true)
  {
    if (xQueueReceive(wifi_msg_queue, &msg, portMAX_DELAY))
    {
      switch (msg.type)
      {
      // --- Commands ---
      case WIFI_CMD_MODE_AP:
        ESP_LOGI(TAG_WIFI, "Switching to AP mode");
        if (is_wifi_connected)
          esp_wifi_disconnect();
        if (is_wifi_started)
          esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_AP);
        wifi_set_config_ap(WIFI_AP_SSID, WIFI_AP_PASSWORD);
        esp_wifi_start();
        break;

      case WIFI_CMD_MODE_AP_STA:
        ESP_LOGI(TAG_WIFI, "Switching to AP+STA mode");
        if (is_wifi_connected)
          esp_wifi_disconnect();
        if (is_wifi_started)
          esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        wifi_set_config_ap(WIFI_AP_SSID, WIFI_AP_PASSWORD);
        wifi_set_config_sta(WIFI_SSID, WIFI_PASSWORD);
        esp_wifi_start();
        break;

      case WIFI_CMD_SET_STA_CREDENTIALS:
        ESP_LOGI(TAG_WIFI, "Connecting with new STA credentials");
        if (is_wifi_connected)
          esp_wifi_disconnect();
        if (is_wifi_started)
          esp_wifi_stop();
        esp_wifi_set_mode(WIFI_MODE_STA);
        wifi_set_config_sta(msg.data.credentials.ssid,
                            msg.data.credentials.password);
        esp_wifi_start();
        break;

      case WIFI_CMD_PING:
      {
        WifiPingRequest_t req;
        strlcpy(req.host, "www.google.com", sizeof(req.host));
        xQueueSend(wifi_ping_queue, &req, 0);
        break;
      }

      // --- Events ---
      case WIFI_EVT_STA_START:
        is_wifi_started = true;
        esp_wifi_connect();
        break;

      case WIFI_EVT_STA_DISCONNECTED:
        is_wifi_connected = false;
        if (++wifi_connect_retries >= WIFI_CONNECT_MAX_RETRIES)
        {
          ESP_LOGW(TAG_WIFI, "Max retries reached, stopping STA");
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
        ESP_LOGW(TAG_WIFI, "Unknown WiFi message %d", msg.type);
        break;
      }
    }
  }
}

/** @brief Wifi Ping Task
 *  @param arg 
 */
static void wifi_ping_task(void *args)
{
  WifiPingRequest_t req;
  while (true)
  {
    if (xQueueReceive(wifi_ping_queue, &req, portMAX_DELAY))
    {
      wifi_ping(req.host);
    }
  }
}

/** @brief Create WiFi Task
 */
void wifi_task_init()
{
  wifi_msg_queue = xQueueCreate(20, sizeof(WifiMsg_t));
  if (wifi_msg_queue == NULL)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create Wifi message queue");
    return;
  }

  wifi_ping_queue = xQueueCreate(4, sizeof(WifiPingRequest_t));
  if (wifi_ping_queue == NULL)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create Wifi ping queue");
    return;
  }

  wifi_initialize();
  
  xTaskCreate(wifi_task, TAG_WIFI, 4096, NULL, 10, NULL);
  xTaskCreate(wifi_ping_task, TAG_PING, 4096, NULL, 5, NULL);
}