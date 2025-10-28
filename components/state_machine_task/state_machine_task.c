#include "state_machine_task.h"
#include "generic_task.h"

#include "gpio_task.h"
#include "wifi_task.h"
#include "wifi_http_server.h"

#include "esp_log.h"

#define DEBUG_MODE

/** @enum eAppState_t
 *  @brief State Machine Application States
 */
typedef enum
{
  STATE_IDLE,
  STATE_PROVISIONING,
  STATE_CONNECTED,
} eAppState_t;

static const char *TAG = "SM";

static GenericTask sm_task;
static httpd_handle_t sm_http_server = NULL;
static eAppState_t current_state = STATE_IDLE;

/** @brief Transition to a new state
 *  @param new_state New state
 */
static void state_machine_enter_state(eAppState_t new_state)
{
  switch (new_state)
  {
  case STATE_IDLE:
    if (sm_http_server)
    {
      http_stop_webserver(sm_http_server);
      sm_http_server = NULL;
    }

#ifdef DEBUG_MODE
    wifi_set_sta_credentials("OctopusChurch", "BishopNemo");
#else
    wifi_set_ap_mode();
#endif
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

  case STATE_CONNECTED:
    gpio_set_state(GPIO_STATE_LED_SOLID);
    if (sm_http_server)
    {
      http_stop_webserver(sm_http_server);
      sm_http_server = NULL;
    }
    wifi_ping("airgahux2exxu-ats.iot.us-east-1.amazonaws.com");
    break;
  }
}

/** @brief State Machine Task message handler
 *  @param self    Pointer to the GenericTask
 *  @param msg_buf Pointer to the received message buffer
 */
static void state_machine_on_message(GenericTask *self, void *msg_buf, size_t msg_len)
{
  AppMsg_t *app_msg = (AppMsg_t*)msg_buf;
  eAppMsgSource_t source = app_msg->source;
  eAppMsgType_t event = app_msg->type;

  ESP_LOGI(TAG, "Received message (type: %d) from %d", event, source);

  switch (current_state)
  {
  case STATE_IDLE:
    if (event == APP_EVT_AP_STARTED)
    {
      ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_PROVISIONING);
      current_state = STATE_PROVISIONING;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_WIFI_CONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_CONNECTED);
      current_state = STATE_CONNECTED;
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
      ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_CONNECTED);
      current_state = STATE_CONNECTED;
      state_machine_enter_state(current_state);
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %d in STATE_PROVISIONING", event);
    }
    break;

  case STATE_CONNECTED:
    if (event == APP_EVT_WIFI_DISCONNECTED)
    {
      ESP_LOGI(TAG, "Transition: %d -> %d", current_state, STATE_IDLE);
      current_state = STATE_IDLE;
      state_machine_enter_state(current_state);
    }
    else if (event == APP_EVT_PING_SUCCESS)
    {
      ESP_LOGI(TAG, "Received Ping Success event");
    }
    else if (event == APP_EVT_PING_TIMEOUT)
    {
      ESP_LOGI(TAG, "Received Ping Timeout event");
    }
    else
    {
      ESP_LOGW(TAG, "Unexpected event %d in STATE_CONNECTED", event);
    }
    break;
  }
}

/** @brief Post an event to the State Machine task
 *  @param event The event to post
 *  @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL
 */
BaseType_t state_machine_post_event(eAppMsgType_t type, eAppMsgSource_t source)
{
  AppMsg_t app_msg = {.type = type, .source = source};
  return generic_task_post_msg(&sm_task, &app_msg, sm_task.item_size);
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

  generic_task_start(&sm_task, 4096, 15);
}