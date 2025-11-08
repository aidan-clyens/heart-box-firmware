#ifndef __MESSAGE_TYPES_H__
#define __MESSAGE_TYPES_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"

// --- Constants ---
#define MAX_SSID_LEN        32
#define MAX_PASSPHRASE_LEN  64
#define MAX_HOSTNAME_LEN    64

static const char *NVS_SSID_KEY = "wifi_ssid";
static const char *NVS_PASSWORD_KEY = "wifi_password";

/** @enum eAppMsgType_t
 *  @brief System-wide application message types
 */
typedef enum
{
  // -- WIFI Commands -- //
  APP_WIFI_CMD_MODE_AP,
  APP_WIFI_CMD_MODE_AP_STA,
  APP_WIFI_CMD_SET_STA_CREDENTIALS,
  APP_WIFI_CMD_PING,

  // -- WIFI Events -- //
  APP_EVT_WIFI_CONNECTED,
  APP_EVT_WIFI_DISCONNECTED,
  APP_EVT_AP_STARTED,
  APP_EVT_PING_SUCCESS,
  APP_EVT_PING_TIMEOUT,

  // -- GPIO Commands -- //
  APP_GPIO_CMD_SET_STATE,

  // -- GPIO Events -- //
  APP_GPIO_EVT_BUTTON_PRESSED,
  APP_GPIO_EVT_BUTTON_RELEASED,

  // -- AWS IoT Commands -- //
  APP_AWS_IOT_CMD_CONNECT,
  APP_AWS_IOT_CMD_START_LISTENING,
  APP_AWS_IOT_CMD_PUBLISH_BUTTON_PRESSED,
  APP_AWS_IOT_CMD_PUBLISH_BUTTON_RELEASED,

  // -- AWS IoT Events -- //
  APP_AWS_IOT_EVT_CONNECTED,
  APP_AWS_IOT_EVT_DISCONNECTED,
  APP_AWS_IOT_EVT_MSG_PRESSED,
  APP_AWS_IOT_EVT_MSG_RELEASED,

  APP_MSG_NONE = -1 /**< Generic "no message" marker */
} eAppMsgType_t;

/** @enum eAppMsgSource_t
 *  @brief Name of the source task a message originated from
 */
typedef enum
{
  APP_SM,
  APP_WIFI,
  APP_GPIO,
  APP_AWS_IOT
} eAppMsgSource_t;

/** @enum eGpioState_t
 *  @brief Name of the GPIO status LED states
 */
typedef enum
{
  GPIO_STATE_LED_BLINK, /**< Blink the status LED */
  GPIO_STATE_LED_SOLID, /**< Keep the status LED solid ON */
  GPIO_STATE_LED_OFF    /**< Turn the status LED OFF */
} eGpioState_t;

/** @struct WifiCredentials_t
 *  @brief Credentials to connect to a WiFi AP
 */
typedef struct
{
  char ssid[MAX_SSID_LEN + 1];
  char password[MAX_PASSPHRASE_LEN + 1];
} WifiCredentials_t;

/** @struct WifiMsg_t
 *  @brief Command message for the WiFi task
 */
typedef struct
{
  eAppMsgType_t type;
  union
  {
    WifiCredentials_t credentials;
    char host[MAX_HOSTNAME_LEN]; /**< Hostname or IP for ping requests */
  } data;
} WifiMsg_t;

/** @struct GpioMsg_t
 *  @brief Command or event message for the GPIO task
 */
typedef struct
{
  eAppMsgType_t type;
  union
  {
    eGpioState_t state; /**< For APP_GPIO_CMD_SET_STATE */
    int button_level;   /**< For APP_GPIO_EVT_BUTTON_PRESSED */
  } data;
} GpioMsg_t;

/** @struct AwsIotMsg_t
 *  @brief  Message structure for AWS IoT task messages
 */
typedef struct
{
  eAppMsgType_t type;
} AwsIotMsg_t;

/** @struct AppMsg_t
 *  @brief Generic wrapper for system-wide messages
 */
typedef struct
{
  eAppMsgType_t type;
  eAppMsgSource_t source;
  union
  {
    WifiMsg_t wifi;
    GpioMsg_t gpio;
    AwsIotMsg_t aws_iot;
  } data;
} AppMsg_t;

#ifdef __cplusplus
}
#endif

#endif // __MESSAGE_TYPES_H__