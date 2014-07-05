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

<<<<<<< HEAD
struct button {
=======
typedef struct button {
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	int id;
	struct gpio gpios[2];	/* GPIO port for first trigger. */
	unsigned int irqs[2];
	s64 hit_time[2];
	int pitch;
	int notevelocity;
} button;

<<<<<<< HEAD
struct cmidid_gpio_state {
=======
typedef struct cmidid_gpio_state {
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	struct button *buttons;
	int num_buttons;
} state;

static state s;

/*
 * Returns the index of a given irq value in the irqs array.
 * Used to get the corresponding gpio from the gpios array.
 * Unfortunatly the function `int irq_to_gpio(int irq)'
 * in linux/gpio.h didn't work for me.
 */
static struct button *get_irq_to_gpio(int irq)
{
	int i;
<<<<<<< HEAD
	for (i = 0; i < state.num_buttons; i++) {
		if (state.buttons[i].irqs[START] == irq ||
		    state.buttons[i].irqs[END] == irq)
			return &state.buttons[i];
=======
	for (i = 0; i < s.num_buttons; i++) {
		if (s.buttons[i].irq_start == irq)
			return &s.buttons[i].gpio_start;
		if (s.buttons[i].irq_end == irq)
			return &s.buttons[i].gpio_end;
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	}
	return NULL;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
<<<<<<< HEAD
	struct button *b;
	info("Interrupt handler called %d: %p.\n", irq, dev_id);
	b = get_irq_to_gpio(irq);
=======
	dbg("Interrupt handler called %d: %p.\n", irq, dev_id);
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e

	if (b != NULL) {
		send_note(b->pitch, 100);
	}
	return IRQ_HANDLED;
}

static bool is_valid(int gpio)
{
	int i;
<<<<<<< HEAD
	for (i = 0; i < state.num_buttons; i++) {
		if (gpio == state.buttons[i].gpios[START].gpio) {
=======
	for (i = 0; i < s.num_buttons; i++) {
		if (gpio == s.buttons[i].gpio_start.gpio) {
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
<<<<<<< HEAD
		if (gpio == state.buttons[i].gpios[END].gpio) {
=======
		if (gpio == s.buttons[i].gpio_end.gpio) {
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
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
<<<<<<< HEAD
=======

>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
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
<<<<<<< HEAD
	state.num_buttons = num_requested_gpios / 3;
	state.buttons =
	    kzalloc(state.num_buttons * sizeof(struct button), GFP_KERNEL);

	if (state.buttons < 0) {
		err("%p. Could not allocate space state.buttons.",
		    state.buttons);
		return -ENOMEM;
=======
	s.num_buttons = num_requested_gpios / 3;
	s.buttons = kzalloc(s.num_buttons * sizeof(struct button), GFP_KERNEL);

	if (s.buttons < 0) {
		err("%p. Could not allocate space s.buttons.\n", s.buttons);
		return s.buttons;
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	}

	for (i = 0; i < s.num_buttons; i++) {
		err = -EINVAL;
<<<<<<< HEAD
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
=======
		s.buttons[i].id = i;
		if (!is_valid(requested_gpios[3 * i])) {
			err("Invalid gpio: %d\n", requested_gpios[3 * i]);
			goto free_buttons;
		}
		s.buttons[i].gpio_start.gpio = requested_gpios[3 * i];
		s.buttons[i].gpio_start.flags = GPIO_MODE;
		if (!is_valid(requested_gpios[3 * i + 1])) {
			err("Invalid gpio: %d\n", requested_gpios[3 * i] + 1);
			goto free_buttons;
		}
		s.buttons[i].gpio_end.gpio = requested_gpios[3 * i + 1];
		s.buttons[i].gpio_end.flags = GPIO_MODE;
		s.buttons[i].pitch = requested_gpios[3 * i + 2];
		dbg("Setting button %d: gpio_start = %d, gpio_end = %d, "
		    "pitch = %d\n", i, s.buttons[i].gpio_start.gpio,
		    s.buttons[i].gpio_end.gpio, s.buttons[i].pitch);

		if ((err =
		     gpio_request_one(s.buttons[i].gpio_start.gpio,
				      s.buttons[i].gpio_start.flags,
				      s.buttons[i].gpio_start.label)) < 0) {
			err("%d. Could not request gpio %d.",
			    err, s.buttons[i].gpio_start.gpio);
			goto free_buttons;
		}
		if ((err =
		     gpio_request_one(s.buttons[i].gpio_end.gpio,
				      s.buttons[i].gpio_end.flags,
				      s.buttons[i].gpio_end.label)) < 0) {
			err("%d. Could not request gpio %d.", err,
			    s.buttons[i].gpio_end.gpio);
			goto free_buttons;
		}

		if ((err = gpio_to_irq(s.buttons[i].gpio_start.gpio)) < 0) {
			err("Could not request irq for gpio %d.\n",
			    s.buttons[i].gpio_start.gpio);
			goto free_buttons;
		}
		s.buttons[i].irq_start = err;

		if ((err = gpio_to_irq(s.buttons[i].gpio_end.gpio)) < 0) {
			err("Could not request irq for gpio %d.\n",
			    s.buttons[i].gpio_end.gpio);
			goto free_buttons;
		}
		s.buttons[i].irq_end = err;

		if ((err =
		     request_irq(s.buttons[i].irq_start, irq_handler,
				 IRQ_TRIGGER, "irq_start", NULL)) < 0) {
			err("Could not request irq %d for button %d.\n",
			    s.buttons[i].irq_start, s.buttons[i].id);
			goto free_buttons;
		}
		if ((err =
		     request_irq(s.buttons[i].irq_end, irq_handler,
				 IRQ_TRIGGER, "irq_end", NULL)) < 0) {
			err("Could not request irq %d for button %d.\n",
			    s.buttons[i].irq_end, s.buttons[i].id);
			goto free_buttons;
		}
	}
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e

	}
	return 0;

 free_buttons:
<<<<<<< HEAD

	for (--i; i > 0; --i) {
		gpio_free_array(state.buttons[i].gpios, 2);
	}

	kfree(state.buttons);
=======
	kfree(s.buttons);
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	return err;
}

void gpio_exit(void)
{
	int i;
	dbg("GPIO component exiting...\n");
<<<<<<< HEAD

	for (i = 0; i < state.num_buttons; i++) {
		free_irq(state.buttons[i].irqs[START], NULL);
		free_irq(state.buttons[i].irqs[END], NULL);
		gpio_free_array(state.buttons[i].gpios, 2);
=======
	kfree(s.buttons);

	for (i = 0; i < s.num_buttons; i++) {
		free_irq(s.buttons[i].irq_start, NULL);
		free_irq(s.buttons[i].irq_end, NULL);
>>>>>>> a3ea9db66863e7af7776c0edd8b05c813b7a818e
	}
	kfree(state.buttons);
}
