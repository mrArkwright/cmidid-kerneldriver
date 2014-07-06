#define DEBUG
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include "cmidid_main.h"
#include "cmidid_gpio.h"
#include "cmidid_midi.h"

/*
 * GPIOs requested via kernel parameter.
 * This is a simple list of gpio ids with an corresponding pitch.
 */
static int gpio_mapping[MAX_REQUEST];
static int gpio_mapping_size;
module_param_array(gpio_mapping, int, &gpio_mapping_size, 0);
MODULE_PARM_DESC(gpio_mapping,
		 "Mapping of GPIOs to Keys. Format: gpio1a, gpio1b, note1, gpio2a, ...");

static unsigned int jitter_res_time = 100000;
module_param(jitter_res_time, uint, 0);
MODULE_PARM_DESC(jitter_res_time,
		 "timing offset before button hits are registered.");

#define START_BUTTON 0
#define END_BUTTON 1

typedef enum {
	KEY_INACTIVE,
	KEY_TOUCHED,
	KEY_PRESSED
} KEY_STATE;

struct key {
	KEY_STATE state;
	struct gpio gpios[2];	/* GPIO port for first trigger. */
	unsigned int irqs[2];
	s64 hit_time[2];
	struct hrtimer hrtimers[2];
	unsigned char note;
	int last_velocity;
};

struct cmidid_gpio_state {
	struct key *keys;
	int num_keys;
	bool button_active_high[2];
};

int get_key_from_irq(int irq, struct key **k_ret, unsigned char *button_ret);

struct cmidid_gpio_state state;

static void handle_button_event(int irq, bool active)
{
	struct key *k;
	unsigned char button;
	unsigned char velocity;

	if (get_key_from_irq(irq, &k, &button) < 0) {
		dbg("irq not assigned to key\n");
		return;
	}

	switch (k->state) {
	case KEY_INACTIVE:
		if ((button == START_BUTTON) && active) {
			//start counting
			k->state = KEY_TOUCHED;
		} else if ((button == START_BUTTON) && !active) {
			note_off(k->note);
		}
		break;
	case KEY_TOUCHED:
		if ((button == START_BUTTON) && !active) {
			note_off(k->note);
			k->state = KEY_INACTIVE;
		} else if ((button == END_BUTTON) && active) {
			//calculate velocity
			velocity = 100;
			note_on(k->note, velocity);
			k->last_velocity = velocity;
			k->state = KEY_PRESSED;
		}
		break;
	case KEY_PRESSED:
		if ((button == START_BUTTON) && !active) {
			note_off(k->note);
			k->state = KEY_INACTIVE;
		} else if ((button == END_BUTTON) && active) {
			note_on(k->note, k->last_velocity);
		}
		break;
	default:
		note_off(k->note);
		k->state = KEY_INACTIVE;
	}

	dbg("key state: %d, button: %d, active: %d, note: %d\n", k->state,
	    button, active, k->note);
}

int get_key_from_irq(int irq, struct key **k_ret, unsigned char *button_ret)
{
	struct key *k;

	for (k = state.keys; k < state.keys + state.num_keys; k++) {
		if (k->irqs[START_BUTTON] == irq) {
			*k_ret = k;
			*button_ret = START_BUTTON;
			return 0;
		}

		if (k->irqs[END_BUTTON] == irq) {
			*k_ret = k;
			*button_ret = END_BUTTON;
			return 0;
		}
	}

	return -1;
}

static struct key *get_key_from_timer(struct hrtimer *timer)
{
	int i;

	for (i = 0; i < state.num_keys; i++) {
		if (&state.keys[i].hrtimers[START_BUTTON] == timer ||
		    &state.keys[i].hrtimers[END_BUTTON] == timer)
			return &state.keys[i];
	}

	return NULL;
}

enum hrtimer_restart timer_irq(struct hrtimer *timer)
{
	struct key *k;
	int gpio_value_start, gpio_value_end;

	k = get_key_from_timer(timer);
	if (k == NULL) {
		err("htimer not found \n");
		return HRTIMER_NORESTART;
	}
	//send_note(90, 100);
	gpio_value_start = gpio_get_value(k->gpios[START_BUTTON].gpio);
	gpio_value_end = gpio_get_value(k->gpios[END_BUTTON].gpio);

	info("TIMER Interrupt handler called %lu: timer value [%d, %d]\n",
	     timer->state, gpio_value_start, gpio_value_end);
	return HRTIMER_NORESTART;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct key *k;
	ktime_t diff;
	int gpio_value_start, gpio_value_end, res;

	ktime_t time = ktime_get();
	k = get_key_from_gpio(irq);

	gpio_value_start = gpio_get_value(k->gpios[START_BUTTON].gpio);
	gpio_value_end = gpio_get_value(k->gpios[END_BUTTON].gpio);

	info("Interrupt handler called %d: gpio value [%d, %d]. (%lld ns)\n",
	     irq, gpio_value_start, gpio_value_end, time.tv64);

	//TODO: into global space :
	diff = ktime_set(0, jitter_res_time);

	if (k != NULL) {
		send_note(k->note, 100);
		res = hrtimer_start(&k->hrtimers[0], diff, HRTIMER_MODE_REL);
		info("hrtimer_start res: :%d\n", res);
	}
	handle_button_event(irq, true);

	return IRQ_HANDLED;
}

static bool is_valid(int gpio)
{
	int i;
	for (i = 0; i < state.num_keys; i++) {
		if (gpio == state.keys[i].gpios[START_BUTTON].gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
		if (gpio == state.keys[i].gpios[END_BUTTON].gpio) {
			dbg("gpio: %d is invalid. It was already used...\n",
			    gpio);
			return false;
		}
	}

	return gpio_is_valid(gpio);
}

int gpio_init(void)
{
	int i;
	struct key *k;
	unsigned int irq;
	int err = 0;

	dbg("GPIO component initializing...\n");

	/* Drop if the array length is invalid. */
	if (gpio_mapping_size <= 0) {
		err("No GPIO_Mapping specified\n");
		return -EINVAL;
	}

	/* Drop if array length is not a multiple of three. */
	if (gpio_mapping_size % 3 != 0) {
		err("Invalid GPIO-Mapping. Argument number not a multiple of 3. Format: gpio1, gpio2, key, ...\n");
		return -EINVAL;
	}

	/* Allocate one button struct for every pair of gpios with pitch. */
	state.num_keys = gpio_mapping_size / 3;
	state.keys = kzalloc(state.num_keys * sizeof(struct key), GFP_KERNEL);

	if (state.keys == NULL) {
		err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < state.num_keys; i++) {
		k = &state.keys[i];

		k->state = KEY_INACTIVE;

		k->last_velocity = 0;

		if (!is_valid(gpio_mapping[3 * i + START_BUTTON])) {
			err("Invalid gpio: %d\n",
			    gpio_mapping[3 * i + START_BUTTON]);
			err = -EINVAL;
			goto free_buttons;
		}
		k->gpios[START_BUTTON].gpio =
		    gpio_mapping[3 * i + START_BUTTON];
		k->gpios[START_BUTTON].flags = GPIOF_IN;
		k->gpios[START_BUTTON].label = "NO_LABEL";

		if (!is_valid(gpio_mapping[3 * i + END_BUTTON])) {
			err("Invalid gpio: %d\n",
			    gpio_mapping[3 * i + END_BUTTON]);
			err = -EINVAL;
			goto free_buttons;
		}
		k->gpios[END_BUTTON].gpio = gpio_mapping[3 * i + END_BUTTON];
		k->gpios[END_BUTTON].flags = GPIOF_IN;
		k->gpios[END_BUTTON].label = "NO_LABEL";

		k->note = gpio_mapping[3 * i + 2];

		dbg("Setting key: gpio_start = %d, gpio_end = %d, note = %d\n",
		    k->gpios[START_BUTTON].gpio, k->gpios[END_BUTTON].gpio,
		    k->note);

		if ((err = gpio_request_array(k->gpios, 2)) < 0) {
			err("Could not request gpio %d or %d.",
			    k->gpios[START_BUTTON].gpio,
			    k->gpios[END_BUTTON].gpio);
			goto free_buttons;
		}

		irq = gpio_to_irq(k->gpios[START_BUTTON].gpio);
		if (irq < 0) {
			err("Could not request irq for gpio %d.\n",
			    k->gpios[START_BUTTON].gpio);
			err = irq;
			goto free_buttons;
		}
		k->irqs[START_BUTTON] = irq;

		hrtimer_init(&state.keys[i].hrtimers[START_BUTTON],
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hrtimer_init(&state.keys[i].hrtimers[END_BUTTON],
			     CLOCK_MONOTONIC, HRTIMER_MODE_REL);

		state.keys[i].hrtimers[START_BUTTON].function = &timer_irq;
		state.keys[i].hrtimers[END_BUTTON].function = &timer_irq;

		dbg("hrtimer initialized\n");
		irq = gpio_to_irq(k->gpios[END_BUTTON].gpio);
		if (irq < 0) {
			err("Could not request irq for gpio %d.\n",
			    k->gpios[END_BUTTON].gpio);
			err = irq;
			goto free_buttons;
		}
		k->irqs[END_BUTTON] = irq;

		if ((err =
		     request_irq(k->irqs[START_BUTTON], irq_handler,
				 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				 "irq_start", NULL)) < 0) {
			err("Could not request irq for key.\n");
			err = -EINVAL;
			goto free_buttons;
		}

		if ((err =
		     request_irq(k->irqs[END_BUTTON], irq_handler,
				 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				 "irq_start", NULL)) < 0) {
			err("Could not request irq for key.\n");
			err = -EINVAL;
			goto free_buttons;
		}
	}

	return 0;

 free_buttons:

	for (--i; i > 0; --i) {
		gpio_free_array(state.keys[i].gpios, 2);
		hrtimer_cancel(&state.keys[i].hrtimers[END_BUTTON]);
		hrtimer_cancel(&state.keys[i].hrtimers[START_BUTTON]);
	}

	kfree(state.keys);

	return err;
}

void gpio_exit(void)
{
	int i;
	dbg("GPIO component exiting...\n");

	for (i = 0; i < state.num_keys; i++) {
		free_irq(state.keys[i].irqs[START_BUTTON], NULL);
		free_irq(state.keys[i].irqs[END_BUTTON], NULL);
		gpio_free_array(state.keys[i].gpios, 2);
		hrtimer_cancel(&state.keys[i].hrtimers[END_BUTTON]);
		hrtimer_cancel(&state.keys[i].hrtimers[START_BUTTON]);
	}

	kfree(state.keys);
}
