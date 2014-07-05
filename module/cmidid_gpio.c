#define DEBUG
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include "cmidid_main.h"
#include "cmidid_gpio.h"
#include "cmidid_midi.h"

#define MAX_REQUEST 128

/*
 * GPIOs requested via kernel parameter.
 * This is a simple list of gpio ids with an corresponding pitch.
 */
int requested_gpios[MAX_REQUEST];
int num_requested_gpios;
module_param_array(requested_gpios, int, &num_requested_gpios, 0);
MODULE_PARM_DESC(requested_gpios, "Ids for the required gpios.");

static struct button {
	int id;
	struct gpio gpio_start;	/* GPIO port for first trigger. */
	struct gpio gpio_end;	/* GPIO port for second trigger. */
	unsigned int irq_start;
	unsigned int irq_end;
	s64 hit_time_start;
	s64 release_time_start;
	s64 hit_time_end;
	s64 release_time_end;
	int pitch;
	int notevelocity;
};

static struct cmidid_gpio_state {
	struct button *buttons;
	int num_buttons;
};

static struct cmidid_gpio_state state;

/*
 * Returns the index of a given irq value in the irqs array.
 * Used to get the corresponding gpio from the gpios array.
 * Unfortunatly the function `int irq_to_gpio(int irq)'
 * in linux/gpio.h didn't work for me.
 */
static struct gpio *get_irq_to_gpio(int irq)
{
	int i;
	for (i = 0; i < state.num_buttons; i++) {
		if (state.buttons[i].irq_start == irq)
			return &state.buttons[i].gpio_start;
		if (state.buttons[i].irq_end == irq)
			return &state.buttons[i].gpio_end;
	}
	return -EINVAL;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	info("Interrupt handler called %d: %p.\n", irq, dev_id);

	send_note(70, 100);

	return IRQ_HANDLED;
}

static bool is_valid(int gpio)
{
	int i;
	for (i = 0; i < state.num_buttons; i++) {
		if (gpio == state.buttons[i].gpio_start.gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
		if (gpio == state.buttons[i].gpio_end.gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
	}

	return gpio_is_valid(gpio);
}

int gpio_init(void)
{
	int i, err;

	dbg("GPIO component initializing...\n");
	dbg("%d GPIOs requested.", num_requested_gpios);

	/* Drop if the array length is invalid. */
	if ((err = num_requested_gpios) <= 0) {
		err("Unable to request gpio. num_requested_gpios = %d\n", err);
		return -EINVAL;
	}
	/* Drop if array length is not a multiple of three. */
	if (num_requested_gpios % 3 != 0) {
		err("Unable to parse gpios/pitches. num_requested_gpios %% 3 != 0\n");
		return -EINVAL;
	}

	/* Allocate one button struct for every pair of gpios with pitch. */
	state.num_buttons = num_requested_gpios / 3;
	state.buttons =
	    kzalloc(state.num_buttons * sizeof(struct button), GFP_KERNEL);

	if (state.buttons < 0) {
		err("%p. Could not allocate space state.buttons.",
		    state.buttons);
		return state.buttons;
	}

	for (i = 0; i < state.num_buttons; i++) {
		err = -EINVAL;
		state.buttons[i].id = i;
		if (!is_valid(requested_gpios[3 * i])) {
			err("Invalid gpio: %d\n", requested_gpios[3 * i]);
			goto free_buttons;
		}
		state.buttons[i].gpio_start.gpio = requested_gpios[3 * i];
		state.buttons[i].gpio_start.flags = GPIO_MODE;
		if (!is_valid(requested_gpios[3 * i + 1])) {
			err("Invalid gpio: %d\n", requested_gpios[3 * i] + 1);
			goto free_buttons;
		}
		state.buttons[i].gpio_end.gpio = requested_gpios[3 * i + 1];
		state.buttons[i].gpio_end.flags = GPIO_MODE;
		state.buttons[i].pitch = requested_gpios[3 * i + 2];
		dbg("Setting button %d: gpio_start = %d, gpio_end = %d, "
		    "pitch = %d\n", i, state.buttons[i].gpio_start.gpio,
		    state.buttons[i].gpio_end.gpio, state.buttons[i].pitch);

		if ((err =
		     gpio_request_one(state.buttons[i].gpio_start.gpio,
				      state.buttons[i].gpio_start.flags,
				      state.buttons[i].gpio_start.label)) < 0) {
			err("%d. Could not request gpio %d.",
			    err, state.buttons[i].gpio_start.gpio);
			goto free_buttons;
		}
		if ((err =
		     gpio_request_one(state.buttons[i].gpio_end.gpio,
				      state.buttons[i].gpio_end.flags,
				      state.buttons[i].gpio_end.label)) < 0) {
			err("%d. Could not request gpio %d.", err,
			    state.buttons[i].gpio_end.gpio);
			goto free_buttons;
		}

		if ((err = gpio_to_irq(state.buttons[i].gpio_start.gpio)) < 0) {
			err("Could not request irq for gpio %d.\n",
			    state.buttons[i].gpio_start.gpio);
			goto free_buttons;
		}
		state.buttons[i].irq_start = err;

		if ((err = gpio_to_irq(state.buttons[i].gpio_end.gpio)) < 0) {
			err("Could not request irq for gpio %d.\n",
			    state.buttons[i].gpio_end.gpio);
			goto free_buttons;
		}
		state.buttons[i].irq_end = err;

		if ((err =
		     request_irq(state.buttons[i].irq_start, irq_handler,
				 IRQ_TRIGGER, "irq_start", NULL)) < 0) {
			err("Could not request irq for button %d.\n",
			    state.buttons[i].id);
			goto free_buttons;
		}
		if ((err =
		     request_irq(state.buttons[i].irq_end, irq_handler,
				 IRQ_TRIGGER, "irq_end", NULL)) < 0) {
			err("Could not request irq for button %d.\n",
			    state.buttons[i].id);
			goto free_buttons;
		}
	}

	return 0;

 free_buttons:
	kfree(state.buttons);
	return err;
}

void gpio_exit(void)
{
	int i;
	dbg("GPIO component exiting...");
	kfree(state.buttons);

	for (i = 0; i < state.num_buttons; i++) {
		free_irq(state.buttons[i].irq_start, NULL);
		free_irq(state.buttons[i].irq_end, NULL);
	}
}
