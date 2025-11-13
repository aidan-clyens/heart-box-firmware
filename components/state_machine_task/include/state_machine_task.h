/**
 * @file state_machine_task.h
 * @brief HeartBox State Machine Task - Application Controller
 * 
 * Coordinates WiFi provisioning, connectivity, and AWS IoT integration.
 * Processes events from GPIO, WiFi, and AWS IoT components.
 * 
 * @note Must be initialized after NVS, GPIO, and WiFi components
 */

#ifndef STATE_MACHINE_TASK_H
#define STATE_MACHINE_TASK_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "message_types.h"

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

/**
 * @brief Initialize and start the State Machine task
 * 
 * Initializes the state machine in IDLE state and creates the FreeRTOS
 * task that processes state transition events. Must be called once during
 * system initialization.
 * 
 * @return ESP_OK on success, error code on failure
 * 
 * @note Call after initializing NVS, GPIO, WiFi, and AWS IoT components
 */
esp_err_t state_machine_task_init(void);

/**
 * @brief Stop and clean up the State Machine task
 * 
 * Gracefully stops the state machine task and frees all allocated resources.
 * After calling this, state_machine_task_init() can be called again to restart.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t state_machine_task_deinit(void);

/**
 * @brief Post an event to the State Machine task
 * 
 * Sends a message to the state machine task queue for processing.
 * This is the primary interface for triggering state transitions.
 * 
 * @param[in] type   The event type (e.g., APP_EVT_WIFI_CONNECTED)
 * @param[in] source The source component that generated the event
 * 
 * @return pdTRUE if the message was successfully posted to the queue
 * @return errQUEUE_FULL if the queue is full (event is lost)
 * 
 * @warning Not ISR-safe. Do not call from interrupt context.
 * @warning If errQUEUE_FULL is returned, the event is lost. Critical events
 *          should implement retry logic.
 * 
 * @note Thread-safe: Can be called from multiple tasks concurrently
 */
BaseType_t state_machine_post_event(eAppMsgType_t type, eAppMsgSource_t source);

/** @brief Get the current application state
 *  @return Current application state
 */
eAppState_t state_machine_get_current_app_state(void);

#ifdef __cplusplus
}
#endif

#endif // STATE_MACHINE_TASK_H