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
static GenericTask *wifi_task = NULL;

static bool is_wifi_started = false;
static bool is_wifi_connected = false;
static int wifi_connect_retries = 0;

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;

/** @brief Post a command message to the WiFi task */
BaseType_t wifi_post_msg(WifiMsg_t msg)
{
  return generic_task_post_msg(wifi_task, &msg, sizeof(WifiMsg_t));
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

/** @brief Public API: Check if WiFi is started
 *  @return true if WiFi is started, false otherwise
 */
bool wifi_task_is_started(void)
{
  return is_wifi_started;
}

/** @brief Public API: Check if WiFi is connected
 *  @return true if WiFi is connected, false otherwise
 */
bool wifi_task_is_connected(void)
{
  return is_wifi_connected;
}

/** @brief Public API: Get the current WiFi mode
 *  @return Current WiFi mode
 */
wifi_mode_t wifi_get_mode(void)
{
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  return mode;
}

/** @brief Public API: Wait for WiFi connection with timeout
 *  @param timeout_ms Maximum time to wait in milliseconds
 *  @return ESP_OK if connected, ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t wifi_wait_for_connection(unsigned int timeout_ms)
{
  const TickType_t start_time = xTaskGetTickCount();
  const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

  while ((xTaskGetTickCount() - start_time) < timeout_ticks)
  {
    if (wifi_task_is_connected())
    {
      return ESP_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  return ESP_ERR_TIMEOUT;
}

/** @brief Event handler for WiFi and IP events
 *  @param arg User-defined argument
 *  @param event_base Event base
 *  @param event_id Event ID
 *  @param event_data Event data
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_STA_START");
      is_wifi_started = true;
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_STA_CONNECTED");
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_STA_DISCONNECTED");
      is_wifi_connected = false;
      if (++wifi_connect_retries >= MAX_WIFI_CONNECT_RETRIES)
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
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_AP_START");
      is_wifi_started = true;
      state_machine_post_event(APP_EVT_AP_STARTED, APP_WIFI);
      break;

    case WIFI_EVENT_STA_STOP:
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_STA_STOP");
      is_wifi_started = false;
      break;

    case WIFI_EVENT_AP_STOP:
      ESP_LOGI(TAG_WIFI, "Received event - WIFI_EVENT_AP_STOP");
      is_wifi_started = false;
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ESP_LOGI(TAG_WIFI, "Received event - IP_EVENT_STA_GOT_IP");
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
  ESP_LOGI(TAG_WIFI, "Ping successful");
  state_machine_post_event(APP_EVT_PING_SUCCESS, APP_WIFI);
}

/** @brief On Ping command timeout callback function
 *  @param hdl Handle for the "ping" object
 *  @param args User arguments
 */
static void wifi_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
  ESP_LOGI(TAG_WIFI, "Ping timeout");
  state_machine_post_event(APP_EVT_PING_TIMEOUT, APP_WIFI);
}

/** @brief On Ping command end callback function
 *  @param hdl Handle for the "ping" object
 *  @param args User arguments
 */
static void wifi_on_ping_end(esp_ping_handle_t hdl, void *args)
{
  ESP_LOGI(TAG_WIFI, "Ping ended");
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
  memset(&target_addr, 0, sizeof(target_addr));
  
  int err = getaddrinfo(hostname, NULL, &hint, &res);
  if (err != 0 || res == NULL)
  {
    ESP_LOGE(TAG_WIFI, "DNS lookup failed for %s: err=%d", hostname, err);
    state_machine_post_event(APP_EVT_PING_TIMEOUT, APP_WIFI);
    return;
  }

  struct sockaddr_in *addr4 = (struct sockaddr_in *)res->ai_addr;
  inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4->sin_addr);
  IP_SET_TYPE(&target_addr, IPADDR_TYPE_V4);
  freeaddrinfo(res);

  ESP_LOGI(TAG_WIFI, "Pinging %s (%s) ...", hostname, ipaddr_ntoa(&target_addr));

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.target_addr = target_addr;
  ping_config.count = 1;
  ping_config.timeout_ms = 5000;
  ping_config.interval_ms = 1000;
  
  esp_ping_callbacks_t cbs = {
      .on_ping_success = wifi_on_ping_success,
      .on_ping_timeout = wifi_on_ping_timeout,
      .on_ping_end = wifi_on_ping_end,
  };

  esp_ping_handle_t ping;
  esp_err_t ret = esp_ping_new_session(&ping_config, &cbs, &ping);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create ping session: %s", esp_err_to_name(ret));
    state_machine_post_event(APP_EVT_PING_TIMEOUT, APP_WIFI);
    return;
  }

  ret = esp_ping_start(ping);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to start ping: %s", esp_err_to_name(ret));
    esp_ping_delete_session(ping);
    state_machine_post_event(APP_EVT_PING_TIMEOUT, APP_WIFI);
    return;
  }
}

/** @brief Initialize WiFi
 *  @param self Pointer to the generic task object for the WiFi task
*/
static esp_err_t wifi_on_init(GenericTask *self)
{
  esp_err_t ret;

  // Step 1: Create STA network interface
  sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == NULL)
  {
    ESP_LOGE(TAG_WIFI, "esp_netif_create_default_wifi_sta failed");
    return ESP_FAIL;
  }

  // Step 2: Create AP network interface
  ap_netif = esp_netif_create_default_wifi_ap();
  if (ap_netif == NULL)
  {
    ESP_LOGE(TAG_WIFI, "esp_netif_create_default_wifi_ap failed");
    ret = ESP_FAIL;
    goto cleanup_sta_netif;
  }

  // Step 3: Initialize WiFi driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_wifi_init failed: %s", esp_err_to_name(ret));
    goto cleanup_ap_netif;
  }

  // Step 4: Register WiFi event handler
  ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_event_handler_instance_register (WIFI_EVENT) failed: %s", esp_err_to_name(ret));
    goto cleanup_wifi;
  }
  ESP_LOGI(TAG_WIFI, "esp_event_handler_instance_register (WIFI_EVENT) succeeded");

  // Step 5: Register IP event handler
  ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_event_handler_instance_register (IP_EVENT) failed: %s", esp_err_to_name(ret));
    goto cleanup_wifi_event;
  }
  ESP_LOGI(TAG_WIFI, "esp_event_handler_instance_register (IP_EVENT) succeeded");

  return ESP_OK;

  // Cleanup path - executed only on error (reverse order of initialization)
cleanup_wifi_event:
  esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
cleanup_wifi:
  esp_wifi_deinit();
cleanup_ap_netif:
  esp_netif_destroy(ap_netif);
  ap_netif = NULL;
cleanup_sta_netif:
  esp_netif_destroy(sta_netif);
  sta_netif = NULL;
  return ret;
}

/** @brief Deinitialize WiFi
 *  @param self Pointer to the generic task object for the WiFi task
*/
static esp_err_t wifi_on_stop(GenericTask *self)
{
  esp_err_t ret;

  // Step 1: Disconnect if connected (releases DHCP lease)
  if (is_wifi_connected)
  {
    ret = esp_wifi_disconnect();
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG_WIFI, "esp_wifi_disconnect failed: %s", esp_err_to_name(ret));
    }
    // Brief delay to allow disconnect to complete
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Step 2: Stop WiFi and wait for completion
  if (is_wifi_started)
  {
    ret = esp_wifi_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED)
    {
      ESP_LOGE(TAG_WIFI, "esp_wifi_stop failed: %s", esp_err_to_name(ret));
    }
    
    // Wait for WiFi to actually stop (WIFI_EVENT_STA_STOP or WIFI_EVENT_AP_STOP)
    // Event handlers MUST remain registered to receive these events
    // Give it up to 2 seconds
    int wait_count = 0;
    while (is_wifi_started && wait_count < 200)
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      wait_count++;
    }
    
    if (is_wifi_started)
    {
      ESP_LOGW(TAG_WIFI, "WiFi did not stop gracefully within timeout");
    }
  }

  // Step 3: Unregister IP event handler (after WiFi stopped)
  ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_event_handler_instance_unregister (IP_EVENT) failed: %s", esp_err_to_name(ret));
  }

  // Step 4: Unregister WiFi event handler (after WiFi stopped)
  ret = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_event_handler_instance_unregister (WIFI_EVENT) failed: %s", esp_err_to_name(ret));
  }

  // Step 5: Clear default WiFi handlers before deinit
  // This prevents "duplicate key" errors when recreating netifs
  if (sta_netif != NULL)
  {
    esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
  }
  if (ap_netif != NULL)
  {
    esp_wifi_clear_default_wifi_driver_and_handlers(ap_netif);
  }

  // Step 6: Deinitialize WiFi driver
  ret = esp_wifi_deinit();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "esp_wifi_deinit failed: %s", esp_err_to_name(ret));
  }

  // Allow WiFi driver cleanup to complete (internal tasks, timers, etc.)
  vTaskDelay(pdMS_TO_TICKS(500));

  // Step 7: Destroy network interfaces
  if (sta_netif != NULL)
  {
    esp_netif_destroy(sta_netif);
    sta_netif = NULL;
  }

  if (ap_netif != NULL)
  {
    esp_netif_destroy(ap_netif);
    ap_netif = NULL;
  }

  // Step 8: Allow ESP-IDF netif registry to fully clear the interface keys
  // This prevents "duplicate key" errors on rapid reinitializations
  vTaskDelay(pdMS_TO_TICKS(200));

  // Reset state flags
  is_wifi_started = false;
  is_wifi_connected = false;
  wifi_connect_retries = 0;

  ESP_LOGI(TAG_WIFI, "WiFi task stopped successfully");
  return ESP_OK;
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
    ESP_LOGI(TAG_WIFI, "Received command - APP_WIFI_CMD_MODE_AP");
    if (is_wifi_connected)
      esp_wifi_disconnect();
    if (is_wifi_started)
      esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_AP);
    wifi_set_config_ap("HeartBox", "password");
    esp_wifi_start();
    break;

  case APP_WIFI_CMD_SET_STA_CREDENTIALS:
    ESP_LOGI(TAG_WIFI, "Received command - APP_WIFI_CMD_SET_STA_CREDENTIALS");
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
    ESP_LOGI(TAG_WIFI, "Received command - APP_WIFI_CMD_PING");
    wifi_ping_start(msg->data.host);
    break;

  default:
    break;
  }
}

/** @brief Initialize and start WiFi Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_task_init(void)
{
  ESP_LOGI(TAG_WIFI, "Initializing WiFi task...");

  // Create GenericTask instance
  wifi_task = generic_task_create(
      TAG_WIFI,
      sizeof(WifiMsg_t),
      wifi_on_init,
      wifi_on_message,
      wifi_on_stop);

  if (wifi_task == NULL)
  {
    ESP_LOGE(TAG_WIFI, "Failed to create WiFi GenericTask");
    return ESP_ERR_NO_MEM;
  }

  // Start the task
  esp_err_t ret = generic_task_start(wifi_task, 4096, 10);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to start WiFi GenericTask: %s", esp_err_to_name(ret));
    generic_task_delete(wifi_task);
    wifi_task = NULL;
    return ret;
  }

  ESP_LOGI(TAG_WIFI, "WiFi task started successfully");
  return ESP_OK;
}

/** @brief Stop and clean up WiFi Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_task_deinit(void)
{
  if (wifi_task == NULL)
  {
    ESP_LOGW(TAG_WIFI, "WiFi task not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG_WIFI, "Stopping WiFi task...");

  // Stop the task
  esp_err_t ret = generic_task_stop(wifi_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to stop WiFi task: %s", esp_err_to_name(ret));
    return ret;
  }

  // Delete the task
  ret = generic_task_delete(wifi_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_WIFI, "Failed to delete WiFi task: %s", esp_err_to_name(ret));
    return ret;
  }

  wifi_task = NULL;
  ESP_LOGI(TAG_WIFI, "WiFi task stopped and cleaned up");
  return ESP_OK;
}