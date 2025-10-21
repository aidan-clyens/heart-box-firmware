#ifndef __GPIO_TASK_H__
#define __GPIO_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

// --- Pin Assignments ---
#define HEART_LED_ARRAY_PIN GPIO_NUM_27
#define LED_STATUS_PIN_1 GPIO_NUM_2
#define LED_STATUS_PIN_2 GPIO_NUM_33
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

/** @enum eGpioState_t
 *  @brief States for the GPIO status LED
 */
typedef enum
{
  GPIO_STATE_LED_BLINK, /**< Blink the status LED */
  GPIO_STATE_LED_SOLID, /**< Keep the status LED solid ON */
  GPIO_STATE_LED_OFF    /**< Turn the status LED OFF */
} eGpioState_t;

/** @enum eGpioMsgType_t
 *  @brief Commands and events for the GPIO task
 */
typedef enum
{
  GPIO_CMD_SET_STATE,     /**< Command: set LED state */
  GPIO_EVT_BUTTON_PRESSED /**< Event: button pressed */
} eGpioMsgType_t;

/** @struct GpioMsg_t
 *  @brief Command or event message for the GPIO task
 */
typedef struct
{
  eGpioMsgType_t type;
  union
  {
    eGpioState_t state; /**< For GPIO_CMD_SET_STATE */
    int button_level;   /**< For GPIO_EVT_BUTTON_PRESSED */
  } data;
} GpioMsg_t;

/** @brief Initialize and start the GPIO task */
void gpio_task_init(void);

/** @brief Post a message to the GPIO task
 *  @param msg The message to post
 *  @return pdTRUE if posted successfully, errQUEUE_FULL otherwise
 */
BaseType_t gpio_post_msg(GpioMsg_t msg);

/** @brief Change the state of the GPIO status LED
 *  @param state The state to change to
 */
void gpio_set_state(eGpioState_t state);

#ifdef __cplusplus
}
#endif

#endif // __GPIO_TASK_H__