#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

#include <linux/gpio.h>

#include "cmidid_main.h"

/*
 * The startup routine for the GPIO component of the module.
 */
int gpio_init(void);


uint32_t set_min_stroke_time(void);
uint32_t set_max_stroke_time(void);

void set_vel_curve_linear(void);
void set_vel_curve_concave(void);
void set_vel_curve_convex(void);
void set_vel_curve_saturated(void);

/*
 * The cleanup routine for the GPIO component of the module.
 */
void gpio_exit(void);

#endif
