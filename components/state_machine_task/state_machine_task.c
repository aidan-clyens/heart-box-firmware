#include "state_machine_task.h"
#include "generic_task.h"

#include "gpio_task.h"
#include "wifi_task.h"
#include "wifi_http_server.h"
#include "aws_iot_task.h"
#include "file_system.h"

#include "esp_log.h"

// #define DEBUG_MODE

#define STATE_MACHINE_TASK_STACK_SIZE 4096
#define STATE_MACHINE_TASK_PRIORITY   15

/** @enum eAppState_t
 *  @brief State Machine Application States
 */
typedef enum
{
  STATE_IDLE,
  STATE_PROVISIONING,
  STATE_WIFI_CONNECTED,
  STATE_AWS_IOT_CONNECTED
} eAppState_t;

static const char *TAG = "STATE_MACHINE_TASK";

static const char *SSID_KEY = "wifi_ssid";
static const char *PASSWORD_KEY = "wifi_password";

static GenericTask sm_task;
static httpd_handle_t sm_http_server = NULL;
static eAppState_t current_state = STATE_IDLE;

/** @brief Public API: Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppMsgType_t type, eAppMsgSource_t source)
{
  if (type < 0 || source < 0)
  {
    ESP_LOGE(TAG, "Invalid event type or source");
    return pdFALSE;
  }
  AppMsg_t app_msg = {.type = type, .source = source};
  return generic_task_post_msg(&sm_task, &app_msg, sm_task.item_size);
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

/** @brief Transition to a new state
 *  @param new_state New state
 */
static void state_machine_enter_state(eAppState_t new_state)
{
  switch (new_state)
  {
  case STATE_IDLE:
    gpio_set_state(GPIO_STATE_LED_OFF);

    if (sm_http_server)
    {
      http_stop_webserver(sm_http_server);
      sm_http_server = NULL;
    }

#ifdef DEBUG_MODE
    file_system_write_string(SSID_KEY, "OctopusChurch");
    file_system_write_string(PASSWORD_KEY, "BishopNemo");
#endif

    // Attempt to read WiFi credentials from file system
    char *ssid = file_system_read_string(SSID_KEY);
    char *password = file_system_read_string(PASSWORD_KEY);

    ESP_LOGI(TAG, "Read WiFi credentials from file system. SSID=%s, Password=%s", ssid, password);

    if (ssid == NULL || password == NULL)
    {
      ESP_LOGW(TAG, "WiFi credentials not found in file system. Staying in IDLE state.");

      // Set up Access Point mode for provisioning
      wifi_set_ap_mode();
    }
    else
    {
      ESP_LOGI(TAG, "WiFi credentials found. Attempting to connect to WiFi in STA mode.");

      wifi_set_sta_credentials(ssid, password);
    }

    free(ssid);
    free(password);
    break;

  case STATE_PROVISIONING:
    gpio_set_state(GPIO_STATE_LED_BLINK);
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

    file_system_write_string(SSID_KEY, (char *)credentials.ssid);
    file_system_write_string(PASSWORD_KEY, (char *)credentials.password);

    wifi_ping("airgahux2exxu-ats.iot.us-east-1.amazonaws.com");
    break;

  case STATE_AWS_IOT_CONNECTED:
    gpio_set_state(GPIO_STATE_LED_SOLID);
    // Start AWS IoT Keep Alive task to listen for incoming messages
    aws_iot_start_listening();

    break;
  }
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

  ESP_LOGI(TAG, "Received message (type: %d) from %d", event, source);

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
      ESP_LOGW(TAG, "Unexpected event %d in STATE_IDLE", event);
    }
    break;

  case STATE_PROVISIONING:
    if (event == APP_EVT_WIFI_CONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %s -> %s", state_machine_state_to_string(current_state), state_machine_state_to_string(STATE_WIFI_CONNECTED));
      current_state = STATE_WIFI_CONNECTED;
      state_machine_enter_state(current_state);
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %d in STATE_PROVISIONING", event);
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
      aws_iot_connect();
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
      ESP_LOGW(TAG, "Unexpected event %d in STATE_WIFI_CONNECTED", event);
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
    else if (event == APP_GPIO_EVT_BUTTON_PRESSED)
    {
      ESP_LOGI(TAG, "Button pressed event received in STATE_AWS_IOT_CONNECTED");
      // Publish a message to AWS IoT when the button is pressed
      aws_iot_publish_button_event();
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %d in STATE_AWS_IOT_CONNECTED", event);
    }
    break;
  }
}

/** @brief Create State Machine Task
 */
void state_machine_task_init(void)
{
  sm_task.name = TAG;
  sm_task.on_init = NULL; // no special init
  sm_task.on_message = state_machine_on_message;
  sm_task.item_size = sizeof(AppMsg_t);

  current_state = STATE_IDLE;
  state_machine_enter_state(current_state);

  generic_task_start(&sm_task, STATE_MACHINE_TASK_STACK_SIZE, STATE_MACHINE_TASK_PRIORITY);
}