#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "esp_err.h"

/** @struct AwsIotStatistics_t 
 *  @brief Structure to hold AWS IoT task statistics
 */
typedef struct
{
  unsigned int connection_attempts;       /**< Number of connection attempts made */
  unsigned int successful_connections;    /**< Number of successful connections */
  unsigned int messages_published;        /**< Total number of messages published */
  unsigned int messages_received;         /**< Total number of messages received */
} AwsIotStatistics_t;

/** @brief Public API: Initialize and start the AWS IoT task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t aws_iot_task_init(void);

/** @brief Public API: Stop and clean up the AWS IoT task
 *  @return ESP_OK on success, error code on failure
 */
esp_err_t aws_iot_task_deinit(void);

/** @brief Public API: Request a connection to the configured AWS IoT broker */
void aws_iot_connect(const char* broker_url);

/** @brief Public API: Subscribe to a topic on AWS IoT */
void aws_iot_subscribe_to_topic(const char* topic);

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

/** @brief: Public API: Wait for AWS IoT listening state with timeout
 *  @param timeout_ms Maximum time to wait in milliseconds
 *  @return ESP_OK if listening, ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t aws_iot_wait_for_listening(unsigned int timeout_ms);

/** @brief Public API: Get the current MQTT broker URL
 *  @return Current MQTT broker URL string
 */
const char *aws_iot_get_mqtt_broker_url(void);

/** @brief Public API: Get the current MQTT topic
 *  @return Current MQTT topic string
 */
const char *aws_iot_get_mqtt_topic(void);

/** @brief Public API: Retrieve AWS IoT task statistics
 *  @return Structure containing AWS IoT task statistics
 */
AwsIotStatistics_t aws_iot_get_statistics(void);

#ifdef __cplusplus
}
#endif

#endif // __AWS_IOT_TASK_H__