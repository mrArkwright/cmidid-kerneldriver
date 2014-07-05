#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

#include <linux/gpio.h>

#include "cmidid_main.h"

/*
 * The direction and initial state for each GPIO pin.
 * Availabe values are defined in `linux/gpio.h'.
 * NOTE: There is no initial state for input pins.
 */
#define GPIO_MODE GPIOF_IN

/*
 * The input on the GPIO pin that triggers an interrupt.
 * You can use a combination of things like:
 * IRQF_TRIGGER_RISING, IRQF_TRIGGER_FALLING, IRQF_TRIGGER_HIGH, etc.
 * Take a look at the defined values in `linux/interrupt.h'.
 */
#define IRQ_TRIGGER IRQF_TRIGGER_RISING

/*
 * The startup routine for the GPIO component of the module.
 */
int gpio_init(void);

/*
 * The cleanup routine for the GPIO component of the module.
 */
void gpio_exit(void);

#endif
