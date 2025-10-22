#ifndef __GPIO_TASK_H__
#define __GPIO_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#include "message_types.h"

// --- Pin Assignments ---
#define HEART_LED_ARRAY_PIN GPIO_NUM_27
#define LED_STATUS_PIN_1 GPIO_NUM_2
#define LED_STATUS_PIN_2 GPIO_NUM_33
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

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