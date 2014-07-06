#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

#include <linux/gpio.h>

#include "cmidid_main.h"

/*
 * The startup routine for the GPIO component of the module.
 */
int gpio_init(void);

/*
 * The cleanup routine for the GPIO component of the module.
 */
void gpio_exit(void);

#endif
