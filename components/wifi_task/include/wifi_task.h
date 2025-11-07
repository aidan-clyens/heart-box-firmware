#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"

#include "message_types.h"

/** @brief Initialize and start the WiFi task
 */
void wifi_task_init(void);

/** @brief Request WiFi AP mode
 */
void wifi_set_ap_mode(void);

/** @brief Set WiFi credentials for STA mode
 *  @param ssid     WiFi SSID
 *  @param password WiFi password
 */
void wifi_set_sta_credentials(const char *ssid, const char *password);

/** @brief Get the current WiFi credentials
 *  @return WiFi SSID and password
 */
WifiCredentials_t wifi_get_current_credentials(void);

/** @brief Send a ping command to an external host
 *  @param hostname     IPv4 or URL for external host
 */
void wifi_ping(const char *hostname);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_TASK_H__