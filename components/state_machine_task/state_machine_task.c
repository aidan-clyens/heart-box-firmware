#include "state_machine_task.h"
#include "generic_task.h"

#include "gpio_task.h"
#include "wifi_task.h"
#include "wifi_http_server.h"
#include "aws_iot_task.h"
#include "file_system.h"

#include "esp_log.h"

// User definitions
#ifndef MQTT_CLIENT_IDENTIFIER
#error "MQTT_CLIENT_IDENTIFIER must be defined in sdkconfig"
#endif // MQTT_CLIENT_IDENTIFIER
#define MQTT_CLIENT_IDENTIFIER_LENGTH ((uint16_t)(sizeof(MQTT_CLIENT_IDENTIFIER) - 1))

#ifndef MQTT_BROKER_ENDPOINT
#error "MQTT_BROKER_ENDPOINT must be defined in sdkconfig"
#endif // MQTT_BROKER_ENDPOINT

#ifndef MQTT_TOPIC
#error "MQTT_TOPIC must be defined in sdkconfig"
#endif // MQTT_TOPIC
#define MQTT_TOPIC_LENGTH ((uint16_t)(sizeof(MQTT_TOPIC) - 1))

// #define DEBUG_MODE

static const char *TAG = "STATE_MACHINE_TASK";

static GenericTask *sm_task = NULL;
static httpd_handle_t sm_http_server = NULL;
static eAppState_t current_state = STATE_IDLE;

/** @brief Public API: Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppMsgType_t type, eAppMsgSource_t source)
{
  if (sm_task == NULL)
  {
    ESP_LOGE(TAG, "State machine task not initialized");
    return pdFALSE;
  }

  if (type < 0 || source < 0)
  {
    ESP_LOGE(TAG, "Invalid event type or source");
    return pdFALSE;
  }
  
  AppMsg_t app_msg = {.type = type, .source = source};
  return generic_task_post_msg(sm_task, &app_msg, sizeof(AppMsg_t));
}

/** @brief Public API: Post an event message to the State Machine task
 *  @param app_msg Pointer to the AppMsg_t to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event_msg(AppMsg_t *app_msg)
{
  if (sm_task == NULL)
  {
    ESP_LOGE(TAG, "State machine task not initialized");
    return pdFALSE;
  }

  if (app_msg == NULL)
  {
    ESP_LOGE(TAG, "Invalid app_msg pointer");
    return pdFALSE;
  }

  return generic_task_post_msg(sm_task, app_msg, sizeof(AppMsg_t));
}

/** @brief Get the current application state
 *  @return Current application state
 */
eAppState_t state_machine_get_current_app_state(void)
{
  return current_state;
}

/** @brief Convert state enum to string
 *  @param state State enum
 *  @return String representation of the state name
 */
static const char *state_machine_state_to_string(eAppState_t state)
{
  switch (state)
  {
  case STATE_IDLE:
    return "IDLE";
  case STATE_PROVISIONING:
    return "PROVISIONING";
  case STATE_WIFI_CONNECTED:
    return "WIFI_CONNECTED";
  case STATE_AWS_IOT_CONNECTED:
    return "AWS_IOT_CONNECTED";
  default:
    return "UNKNOWN";
  }
}

/** @brief Convert message type enum for events to string
 *  @param msg_type Message type enum
 *  @return String representation of the event message type name
 */
static const char *state_machine_event_to_string(eAppMsgType_t msg_type)
{
  switch (msg_type)
  {
    case APP_EVT_WIFI_CONNECTED:
      return "EVT_WIFI_CONNECTED";
    case APP_EVT_WIFI_DISCONNECTED:
      return "EVT_WIFI_DISCONNECTED";
    case APP_EVT_AP_STARTED:
      return "EVT_AP_STARTED";
    case APP_EVT_PING_SUCCESS:
      return "EVT_PING_SUCCESS";
    case APP_EVT_PING_TIMEOUT:
      return "EVT_PING_TIMEOUT";
    case APP_GPIO_EVT_BUTTON_PRESSED:
      return "GPIO_EVT_BUTTON_PRESSED";
    case APP_GPIO_EVT_BUTTON_RELEASED:
      return "GPIO_EVT_BUTTON_RELEASED";
    case APP_AWS_IOT_EVT_CONNECTED:
      return "AWS_IOT_EVT_CONNECTED";
    case APP_AWS_IOT_EVT_DISCONNECTED:
      return "AWS_IOT_EVT_DISCONNECTED";
    case APP_AWS_IOT_EVT_MSG_PRESSED:
      return "AWS_IOT_EVT_MSG_PRESSED";
    case APP_MSG_NONE:
    default:
      return "MSG_NONE";
  }
}

/** @brief Convert source enum to string
 *  @param source Source enum
 *  @return String representation of the source name
 */
static const char *state_machine_source_to_string(eAppMsgSource_t source)
{
  switch (source)
  {
    case APP_SM:
      return "SM";
    case APP_WIFI:
      return "WIFI";
    case APP_GPIO:
      return "GPIO";
    case APP_AWS_IOT:
      return "AWS_IOT";
    default:
      return "UNKNOWN";
  }
}

/** @brief Transition to a new state
 *  @param new_state New state
 */
static void state_machine_enter_state(eAppState_t new_state)
{
  switch (new_state)
  {
  case STATE_IDLE:
    gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_OFF);

    if (sm_http_server)
    {
      http_stop_webserver(sm_http_server);
      sm_http_server = NULL;
    }

#ifdef DEBUG_MODE
    file_system_write_string(NVS_SSID_KEY, "OctopusChurch");
    file_system_write_string(NVS_PASSWORD_KEY, "BishopNemo");
#endif

    // Attempt to read WiFi credentials from file system
    char *ssid = file_system_read_string(NVS_SSID_KEY);
    char *password = file_system_read_string(NVS_PASSWORD_KEY);
    
    if (ssid == NULL || password == NULL)
    {
      ESP_LOGW(TAG, "WiFi credentials not found in file system. Changing to PROVISIONING state.");
      
      // Set up Access Point mode for provisioning
      wifi_set_ap_mode();
    }
    else
    {
      ESP_LOGI(TAG, "Read WiFi credentials from file system. SSID=%s, Password=%s", ssid, password);
      ESP_LOGI(TAG, "Attempting to connect to WiFi in STA mode.");

      wifi_set_sta_credentials(ssid, password);
    }

    free(ssid);
    free(password);
    break;

  case STATE_PROVISIONING:
    gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_BLINK);
    sm_http_server = http_start_webserver();
    if (sm_http_server == NULL)
    {
      ESP_LOGE(TAG, "Failed to start HTTP server");
    }
    else
    {
      ESP_LOGI(TAG, "HTTP server started");
    }
    break;

  case STATE_WIFI_CONNECTED:
    if (sm_http_server)
    {
      http_stop_webserver(sm_http_server);
      sm_http_server = NULL;
    }

    // Save WiFi credentials to file system
    WifiCredentials_t credentials = wifi_get_current_credentials();

    ESP_LOGI(TAG, "Saving WiFi credentials to file system. SSID=%s, Password=%s", credentials.ssid, credentials.password);

    file_system_write_string(NVS_SSID_KEY, (char *)credentials.ssid);
    file_system_write_string(NVS_PASSWORD_KEY, (char *)credentials.password);

    wifi_ping("8.8.8.8");
    break;

  case STATE_AWS_IOT_CONNECTED:
    aws_iot_subscribe_to_topic(MQTT_TOPIC);
    break;
  }
}

static esp_err_t state_machine_on_init(GenericTask *self)
{
  (void)self; // Not used
  
  current_state = STATE_IDLE;
  state_machine_enter_state(current_state);
  
  ESP_LOGI(TAG, "State Machine Task initialized");
  return ESP_OK;
}

static esp_err_t state_machine_on_stop(GenericTask *self)
{
  (void)self; // Not used

  if (sm_http_server)
  {
    http_stop_webserver(sm_http_server);
    sm_http_server = NULL;
  }

  ESP_LOGI(TAG, "State Machine Task stopped");
  return ESP_OK;
}

/** @brief State Machine Task message handler
 *  @param self    Pointer to the GenericTask
 *  @param msg_buf Pointer to the received message buffer
 */
static void state_machine_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  (void)msg_len; // Not used

  if (msg_buf == NULL || msg_len < sizeof(AppMsg_t))
  {
    ESP_LOGE(TAG, "Invalid message buffer");
    return;
  }

  AppMsg_t *app_msg = (AppMsg_t*)msg_buf;
  eAppMsgSource_t source = app_msg->source;
  eAppMsgType_t event = app_msg->type;

  ESP_LOGI(TAG, "Received message (type: %s) from %s", state_machine_event_to_string(event), state_machine_source_to_string(source));

  switch (current_state)
  {
  case STATE_IDLE:
    if (event == APP_EVT_AP_STARTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_PROVISIONING));
      current_state = STATE_PROVISIONING;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_WIFI_CONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_WIFI_CONNECTED));
      current_state = STATE_WIFI_CONNECTED;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_WIFI_DISCONNECTED)
    {
      ESP_LOGI(TAG, "WiFi Disconnected");
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %s in STATE_IDLE", state_machine_event_to_string(event));
    }
    break;

  case STATE_PROVISIONING:
    if (event == APP_EVT_WIFI_CONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_WIFI_CONNECTED));
      current_state = STATE_WIFI_CONNECTED;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_WIFI_DISCONNECTED)
    {
      ESP_LOGI(TAG, "WiFi connection attempt failed in PROVISIONING state... Remaining in PROVISIONING state");
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %s in STATE_PROVISIONING", state_machine_event_to_string(event));
    }
    break;

  case STATE_WIFI_CONNECTED:
    if (event == APP_EVT_WIFI_DISCONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_IDLE));
      current_state = STATE_IDLE;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_PING_SUCCESS)
    {
      ESP_LOGI(TAG, "Received Ping Success event");
      aws_iot_connect(MQTT_BROKER_ENDPOINT, MQTT_CLIENT_IDENTIFIER);
    }
    else if (event == APP_EVT_PING_TIMEOUT)
    {
      ESP_LOGI(TAG, "Received Ping Timeout event");
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_IDLE));
      current_state = STATE_IDLE;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_AWS_IOT_EVT_CONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_AWS_IOT_CONNECTED));
      current_state = STATE_AWS_IOT_CONNECTED;
      state_machine_enter_state(current_state);
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %s in STATE_WIFI_CONNECTED", state_machine_event_to_string(event));
    }
    break;

  case STATE_AWS_IOT_CONNECTED:
    if (event == APP_EVT_WIFI_DISCONNECTED)
    {
      ESP_LOGI(TAG, "Received WiFi Disconnected event");
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_IDLE));
      current_state = STATE_IDLE;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_AWS_IOT_EVT_DISCONNECTED)
    {
      ESP_LOGI(TAG, "Received AWS IoT Disconnected event");
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_IDLE));
      current_state = STATE_IDLE;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_AWS_IOT_EVT_SUBSCRIBED)
    {
      ESP_LOGI(TAG, "Successfully subscribed to AWS IoT topic %s in STATE_AWS_IOT_CONNECTED", MQTT_TOPIC);

      gpio_set_state(LED_STATUS_PIN_2, GPIO_STATE_LED_SOLID);

      // Start AWS IoT Keep Alive task to listen for incoming messages
      aws_iot_start_listening();
    }
    else if (event == APP_GPIO_EVT_BUTTON_PRESSED)
    {
      ESP_LOGI(TAG, "Button pressed event received in STATE_AWS_IOT_CONNECTED");
      // Publish a message to AWS IoT when the button is pressed
      GpioMsg_t *gpio_msg = &app_msg->data.gpio;
      aws_iot_publish_button_event(gpio_msg->data.button_event.duration_ms);
    }
    else if (event == APP_AWS_IOT_EVT_MSG_PRESSED)
    {
      ESP_LOGI(TAG, "Message received from AWS IoT in STATE_AWS_IOT_CONNECTED");
      gpio_set_state(HEART_LED_ARRAY_PIN, GPIO_STATE_LED_SOLID);
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %s in STATE_AWS_IOT_CONNECTED", state_machine_event_to_string(event));
    }
    break;
  }
}

/** @brief Initialize and start State Machine Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t state_machine_task_init(void)
{
  ESP_LOGI(TAG, "Initializing State Machine task...");

  // Create GenericTask instance
  sm_task = generic_task_create(
      TAG,
      sizeof(AppMsg_t),
      state_machine_on_init,
      state_machine_on_message,
      state_machine_on_stop);

  if (sm_task == NULL)
  {
    ESP_LOGE(TAG, "Failed to create State Machine GenericTask");
    return ESP_ERR_NO_MEM;
  }

  // Start the task
  esp_err_t ret = generic_task_start(sm_task, 4096, 15);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to start State Machine GenericTask: %s", esp_err_to_name(ret));
    generic_task_delete(sm_task);
    sm_task = NULL;
    return ret;
  }

  ESP_LOGI(TAG, "State Machine task started successfully");
  return ESP_OK;
}

/** @brief Stop and clean up State Machine Task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t state_machine_task_deinit(void)
{
  if (sm_task == NULL)
  {
    ESP_LOGW(TAG, "State Machine task not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Stopping State Machine task...");

  // Stop the task
  esp_err_t ret = generic_task_stop(sm_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to stop State Machine task: %s", esp_err_to_name(ret));
    return ret;
  }

  // Delete the task
  ret = generic_task_delete(sm_task);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to delete State Machine task: %s", esp_err_to_name(ret));
    return ret;
  }

  sm_task = NULL;
  ESP_LOGI(TAG, "State Machine task stopped and cleaned up");
  return ESP_OK;
}