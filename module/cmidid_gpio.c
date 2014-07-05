#define DEBUG
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include "cmidid_main.h"
#include "cmidid_gpio.h"
#include "cmidid_midi.h"

extern int num_requested_gpios;
extern int requested_gpios[MAX_REQUEST];

#define START 0
#define END 1

struct button {
	int id;
	struct gpio gpios[2];	/* GPIO port for first trigger. */
	unsigned int irqs[2];
	s64 hit_time[2];
	int pitch;
	int notevelocity;
};

struct cmidid_gpio_state {
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
static struct button *get_irq_to_gpio(int irq)
{
	int i;
	for (i = 0; i < state.num_buttons; i++) {
		if (state.buttons[i].irqs[START] == irq ||
		    state.buttons[i].irqs[END] == irq)
			return &state.buttons[i];
	}
	return NULL;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct button *b;
	info("Interrupt handler called %d: %p.\n", irq, dev_id);
	b = get_irq_to_gpio(irq);

	if (b != NULL) {
		send_note(b->pitch, 100);
	}
	return IRQ_HANDLED;
}

static bool is_valid(int gpio)
{
	int i;
	for (i = 0; i < state.num_buttons; i++) {
		if (gpio == state.buttons[i].gpios[START].gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
		if (gpio == state.buttons[i].gpios[END].gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
	}

	return gpio_is_valid(gpio);
}

int gpio_init(void)
{
	int i, j, err;

	dbg("GPIO component initializing...\n");
	dbg("%d GPIOs requested.\n", num_requested_gpios);
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
		return -ENOMEM;
	}

	for (i = 0; i < state.num_buttons; i++) {
		err = -EINVAL;
		state.buttons[i].id = i;
		for (j = 0; j < 2; j++) {
			if (!is_valid(requested_gpios[3 * i + j])) {
				err("%d. Invalid gpio: %d\n", err,
				    requested_gpios[3 * i + j]);
				goto free_buttons;
			}
			state.buttons[i].gpios[j].gpio =
			    requested_gpios[3 * i + j];
			state.buttons[i].gpios[j].flags = GPIO_MODE;
			state.buttons[i].gpios[j].label = "NO_LABEL";
		}

		state.buttons[i].pitch = requested_gpios[3 * i + 2];
		dbg("Setting button %d: gpio_start = %d, gpio_end = %d, "
		    "pitch = %d\n", i, state.buttons[i].gpios[START].gpio,
		    state.buttons[i].gpios[END].gpio, state.buttons[i].pitch);

		if ((err = gpio_request_array(state.buttons[i].gpios, 2)) < 0) {
			err("%d. Could not request gpio %d or %d.",
			    err, state.buttons[i].gpios[START].gpio,
			    state.buttons[i].gpios[END].gpio);
			goto free_buttons;
		}
		for (j = 0; j < 2; j++) {
			if ((err =
			     gpio_to_irq(state.buttons[i].gpios[j].gpio)) < 0) {
				err("Could not request irq for gpio %d.\n",
				    state.buttons[i].gpios[j].gpio);
				goto free_buttons;
			}
			state.buttons[i].irqs[j] = err;
		}
		for (j = 0; j < 2; j++) {
			if ((err =
			     request_irq(state.buttons[i].irqs[j], irq_handler,
					 IRQ_TRIGGER, "irq_start", NULL)) < 0) {
				err("Could not request irq for button %d.\n",
				    state.buttons[i].id);
				goto free_buttons;
			}
		}

	}
	return 0;

 free_buttons:

	for (--i; i > 0; --i) {
		gpio_free_array(state.buttons[i].gpios, 2);
	}

	kfree(state.buttons);
	return err;
}

void gpio_exit(void)
{
	int i;
	dbg("GPIO component exiting...\n");

	for (i = 0; i < state.num_buttons; i++) {
		free_irq(state.buttons[i].irqs[START], NULL);
		free_irq(state.buttons[i].irqs[END], NULL);
		gpio_free_array(state.buttons[i].gpios, 2);
	}
	kfree(state.buttons);
}
