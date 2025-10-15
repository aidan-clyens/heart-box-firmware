#ifndef __GPIO_TASK_H__
#define __GPIO_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"

#define HEART_LED_ARRAY_PIN GPIO_NUM_27
#define LED_STATUS_PIN_1 GPIO_NUM_2
#define LED_STATUS_PIN_2 GPIO_NUM_33
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

/** @enum eGpioState_t
 *  @brief States for the GPIO task
 */
typedef enum
{
  GPIO_STATE_LED_BLINK,
  GPIO_STATE_LED_SOLID,
  GPIO_STATE_LED_OFF,
} eGpioState_t;

void gpio_task_init();
void gpio_set_state(eGpioState_t state);

#ifdef __cplusplus
}
#endif

#endif // __GPIO_TASK_H__