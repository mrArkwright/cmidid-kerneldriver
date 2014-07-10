#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

/* Maximum number of keys that can be specified in gpio_mapping param. */
#define MAX_KEYS 90

uint32_t set_min_stroke_time(void);
uint32_t set_max_stroke_time(void);

void set_vel_curve_linear(void);
void set_vel_curve_concave(void);
void set_vel_curve_convex(void);
void set_vel_curve_saturated(void);

int gpio_init(void);
void gpio_exit(void);

#endif
