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