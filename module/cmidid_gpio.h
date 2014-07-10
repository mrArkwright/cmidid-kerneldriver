#ifndef CMIDID_GPIO_H
#define CMIDID_GPIO_H

/* Maximum number of keys that can be specified in gpio_mapping param. */
#define MAX_KEYS 88

uint32_t cmidid_set_min_stroke_time(void);
uint32_t cmidid_set_max_stroke_time(void);

void cmidid_set_vel_curve_linear(void);
void cmidid_set_vel_curve_concave(void);
void cmidid_set_vel_curve_convex(void);
void cmidid_set_vel_curve_saturated(void);

int cmidid_gpio_init(void);
void cmidid_gpio_exit(void);

#endif
