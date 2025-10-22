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

/** @enum eAppMsgType_t
 *  @brief System-wide application message types
 */
typedef enum
{
  // -- State Machine -- //
  APP_EVT_WIFI_CONNECTED,
  APP_EVT_WIFI_DISCONNECTED,
  APP_EVT_AP_STARTED,
  APP_EVT_BUTTON_PRESSED,

  // -- WIFI -- //
  APP_WIFI_MSG_NONE = -1,
  APP_WIFI_CMD_MODE_AP,
  APP_WIFI_CMD_MODE_AP_STA,
  APP_WIFI_CMD_SET_STA_CREDENTIALS,
  APP_WIFI_CMD_PING,
  APP_WIFI_EVT_STA_START,
  APP_WIFI_EVT_STA_DISCONNECTED,
  APP_WIFI_EVT_AP_START,
  APP_WIFI_EVT_AP_STOP,
  APP_WIFI_EVT_STA_GOT_IP,

  // -- GPIO -- //
  APP_GPIO_CMD_SET_STATE,      /**< Command: set LED state */
  APP_GPIO_EVT_BUTTON_PRESSED, /**< Event: button pressed */

  // -- MQTT -- //
  APP_MQTT_CMD_CONNECT,
  APP_MQTT_CMD_DISCONNECT,
  APP_MQTT_EVT_CONNECTED,
  APP_MQTT_EVT_DISCONNECTED,
  APP_MQTT_EVT_ERROR,
} eAppMsgType_t;

typedef enum
{
  APP_SM,
  APP_WIFI,
  APP_GPIO,
  APP_MQTT
} eAppMsgSource_t;

/** @enum eGpioState_t
 *  @brief States for the GPIO status LED
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
 *  @brief Command or event message for the WiFi task
 */
typedef struct
{
  eAppMsgType_t type;
  union
  {
    WifiCredentials_t credentials;
    char host[64]; /**< Hostname or IP for ping requests */
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

/** @struct MqttMsg_t
 *  @brief Command or event message for the MQTT task
 */
typedef struct
{
  eAppMsgType_t type;
  union
  {
    int error_code;
  } data;
} MqttMsg_t;

typedef struct
{
  eAppMsgType_t type;
  eAppMsgSource_t source;
  union
  {
    WifiMsg_t wifi;
    GpioMsg_t gpio;
    MqttMsg_t mqtt;
  } data;
} AppMsg_t;

#ifdef __cplusplus
}
#endif

#endif // __MESSAGE_TYPES_H__