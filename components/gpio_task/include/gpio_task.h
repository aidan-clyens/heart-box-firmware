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

//  --- GPIO Level Definitions ---
#define GPIO_HIGH 1
#define GPIO_LOW 0

//  --- Time Definitions ---
#define GPIO_LED_BLINK_INTERVAL_MS 1000
#define BUTTON_DEBOUNCE_TIME_MS 50

#define DEBUG_GPIO_BUTTON_ISR

/** @brief Initialize and start the GPIO task */
void gpio_task_init(void);

/** @brief Change the state of the GPIO status LED
 *  @param state The state to change to
 */
void gpio_set_state(eGpioState_t state);

/** @brief Get the current level of the status LED
 *  @return The current level of the status LED (0 = OFF, 1 = ON)
 */
unsigned int gpio_get_status_led_level(void);

/** @brief Get the current level of the output LED
 *  @return The current level of the output LED (0 = LOW, 1 = HIGH)
 */
unsigned int gpio_get_output_led_level(void);

#ifdef DEBUG_GPIO_BUTTON_ISR
/** @brief Simulate a button press ISR for testing purposes */
void gpio_simulate_button_press_isr(void);

/** @brief Set the simulated button level for testing purposes
 *  @param level The level to set (0 = LOW, 1 = HIGH)
 */
void gpio_set_button_level(int level);
#endif

#ifdef __cplusplus
}
#endif

#endif // __GPIO_TASK_H__