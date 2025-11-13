#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "esp_err.h"

/** @brief Public API: Initialize and start the AWS IoT task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t aws_iot_task_init(void);

/** @brief Public API: Stop and clean up the AWS IoT task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t aws_iot_task_deinit(void);

/** @brief Public API: Request a connection to the configured AWS IoT broker */
void aws_iot_connect(void);

/** @brief Public API: Start listening for incoming MQTT messages from AWS IoT */
void aws_iot_start_listening(void);

/** @brief Public API: Publish a button press event to AWS IoT */
void aws_iot_publish_button_pressed_event(void);

/** @brief Public API: Publish a button released event to AWS IoT */
void aws_iot_publish_button_released_event(void);

/** @brief Public API: Get the current MQTT connection status
 *  @return Current MQTT connection status
 */
bool aws_iot_is_connected(void);

/** @brief Public API: Check if AWS IoT task is listening for messages
 *  @return true if listening, false otherwise
 */
bool aws_iot_is_listening(void);

/** @brief: Public API: Wait for AWS IoT connection with timeout
 *  @param timeout_ms Maximum time to wait in milliseconds
 *  @return ESP_OK if connected, ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t aws_iot_wait_for_connection(unsigned int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // __AWS_IOT_TASK_H__