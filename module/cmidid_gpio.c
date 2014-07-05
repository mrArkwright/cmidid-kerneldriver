#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "cmidid_main.h"
#include "cmidid_gpio.h"
#include "cmidid_midi.h"

struct gpio gpios[NUM_GPIOS];

struct button buttons[ARRAY_SIZE(gpios)];

int irqs[ARRAY_SIZE(gpios)];

int button_map[ARRAY_SIZE(gpios)];

module_param_array(button_map, int, NULL, 0);
MODULE_PARM_DESC(button_map, "Mapping from GPIO numbers to logical keyboard "
		 "buttons. The GPIO port i will be mapped to the logical "
		 "button button_map[i].");

/*
 * The minimum time interval that has to pass before a button hit_time
 * for a single button is updated. This should probably be within
 * millisecond range.
 *
 * Unfortunately, 64 Byte types are not supported as module parameters.
 */
static int hit_time_threshold;

module_param(hit_time_threshold, int, PARM_VIS_FLAGS);
MODULE_PARM_DESC(hit_time_threshold, "Minimum time (in ns) between two "
		 "registered key hits.");

/*
 * Returns the index of a given irq value in the irqs array.
 * Used to get the corresponding gpio from the gpios array.
 * Unfortunatly the function `int irq_to_gpio(int irq)'
 * in linux/gpio.h didn't work for me.
 */
static int get_irq_index(int irq)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		if (irqs[i] == irq)
			return i;
	}
	return -EINVAL;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	int gpio = irq_to_gpio(irq);

	return IRQ_HANDLED;
}

int gpio_init(void)
{
	int i, ret;
	char gpio_name[8];

	size_t num_gpios = ARRAY_SIZE(gpios);

	info("GPIO component initializing...\n");
	info("Using %lu GPIOs: 0 - %lu\n", num_gpios, num_gpios - 1);

	/* Initialize the gpios array. */
	for (i = 0; i < num_gpios; i++) {
		snprintf(gpio_name, ARRAY_SIZE(gpio_name), "GPIO %02d", i);
		gpios[i] = (struct gpio) {
		.gpio = i,.flags = GPIO_MODE,.label = gpio_name,};
	}

	/* Initialize the irqs and irq_times arrays. */
	for (i = 0; i < ARRAY_SIZE(irqs); i++)
		irqs[i] = -1;

	return 0;

	/* Request an array of GPIOs. */
	if ((ret = gpio_request_array(gpios, ARRAY_SIZE(gpios)))) {
		err("%d. Unable to request GPIOs for 'gpios' array.", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		/* Request IRQs for every GPIO port. */
		if ((ret = gpio_to_irq(gpios[i].gpio)) < 0) {
			err("%d. Unable to request IRQ for gpio: %d\n",
			    ret, gpios[i].gpio);
			return ret;
		}
		info("Setting IRQ %d for gpio %d\n", ret, gpios[i].gpio);
		irqs[i] = ret;
		/* Set the interrupt service routine for every port. */
		/* Add IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING. */
		ret = request_irq(irqs[i], irq_handler,
				  IRQF_TRIGGER_FALLING |
				  IRQF_DISABLED, gpios[i].label, 0);
		if (ret) {
			err("%d. Unable to request IRQ for port %d\n", ret,
			    gpios[i].gpio);
			return ret;
		}
	}

	return 0;
}

void gpio_exit(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		free_irq(irqs[i], 0);
	}
	gpio_free_array(gpios, ARRAY_SIZE(gpios));

}
