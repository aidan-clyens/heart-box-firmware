#ifndef __GPIO_TASK_H__
#define __GPIO_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"

#define HEART_LED_ARRAY_PIN GPIO_NUM_14
#define LED_STATUS_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_25

#define BUTTON_DEBOUNCE_TIME_MS 50

void gpio_initialize();
void gpio_create_task();

#ifdef __cplusplus
}
#endif

#endif // __GPIO_TASK_H__