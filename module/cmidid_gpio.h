#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

#include <linux/gpio.h>
#include <linux/moduleparam.h>

#include "cmidid_main.h"

/*
 * The total number of available GPIOs for our module.
 * Adjust according to the Raspberry Pi version you use.
 */
#define NUM_GPIOS 24

/*
 * The array of used GPIO pins.
 */
extern struct gpio gpios[NUM_GPIOS];

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
 * The array of IRQ values/numbers.
 * Numbers are assigned automatically by the `gpio_init()' routine.
 */
extern int irqs[ARRAY_SIZE(gpios)];

/*
 * Represents a single button for a keyboard key.
 * The hit_time stores the last time (in ns) the button was pressed/hit.
 */
struct button {
        int id;
        s64 hit_time;
};

/*
 * The array of all buttons for a custom MIDI keyboard.
 * Every pair of two successive buttons in this array is associated
 * with a single keyboard key.
 */
extern struct button buttons[ARRAY_SIZE(gpios)];

/*
 * The startup routine for the GPIO component of the module.
 */
int gpio_init(void);

/*
 * The cleanup routine for the GPIO component of the module.
 */
void gpio_exit(void);

#endif
