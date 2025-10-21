#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"

// --- Constants ---
#define MAX_SSID_LEN        32
#define MAX_PASSPHRASE_LEN  64

/** @enum eWifiMsgType_t
 *  @brief Commands and events for the WiFi task
 */
typedef enum
{
  WIFI_MSG_NONE = -1,
  WIFI_CMD_MODE_AP,
  WIFI_CMD_MODE_AP_STA,
  WIFI_CMD_SET_STA_CREDENTIALS,
  WIFI_CMD_PING,
  WIFI_EVT_STA_START,
  WIFI_EVT_STA_DISCONNECTED,
  WIFI_EVT_AP_START,
  WIFI_EVT_AP_STOP,
  WIFI_EVT_STA_GOT_IP,
} eWifiMsgType_t;

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
  eWifiMsgType_t type;
  union
  {
    WifiCredentials_t credentials;
    char host[64];   /**< Hostname or IP for ping requests */
  } data;
} WifiMsg_t;

/** @brief Initialize and start the WiFi task
 */
void wifi_task_init(void);

/** @brief Post a message to the WiFi task
 *  @param msg The message to post
 *  @return pdTRUE if posted successfully, errQUEUE_FULL otherwise
 */
BaseType_t wifi_post_msg(WifiMsg_t msg);

/** @brief Request WiFi AP mode
 */
void wifi_set_ap_mode(void);

/** @brief Set WiFi credentials for STA mode
 *  @param ssid     WiFi SSID
 *  @param password WiFi password
 */
void wifi_set_sta_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__