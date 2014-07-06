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
 * The format for passing the values is:
 * gpio_mapping=gpio1a,gpio1b,note1,gpio2a,gpio2b,note2
 */
static int gpio_mapping[MAX_REQUEST];
static int gpio_mapping_size;
module_param_array(gpio_mapping, int, &gpio_mapping_size, 0);
MODULE_PARM_DESC(gpio_mapping,
		 "Mapping of GPIOs to Keys. Format: gpio1a, gpio1b, note1, gpio2a, ...");

/*
 * The time offset before every MIDI event is sent.
 * Delaying the sending of MIDI events while ignoring subsequent button
 * hits within the this time period can be used to counter button jittering.
 *
 * According to some testing we did, this value should be at least ~500us
 * and probably less than ~10ms. Lower values will possibly result in
 * jittering/bouncing and higher values could block legitimate button hits.
 */
static unsigned int jitter_res_time = 1000000;
module_param(jitter_res_time, uint, 0);
MODULE_PARM_DESC(jitter_res_time,
		 "timing offset before button hits are registered.");

/*
 * START_BUTTON and END_BUTTON are used to index the GPIO buttons
 * in every key struct. START_BUTTON is the id for the button
 * which is hit first when the keyboard key is pressed down.
 * When the END_BUTTON is hit, the key should be completely pressed down.
 */
#define START_BUTTON 0
#define END_BUTTON 1

/*
 * Possible states for every button of our MIDI keyboard.
 */
typedef enum {
	KEY_INACTIVE,
	KEY_TOUCHED,
	KEY_PRESSED
} KEY_STATE;

typedef enum {
	VEL_CURVE_LINEAR,
	VEL_CURVE_CONCAVE,
	VEL_CURVE_CONVEX,
	VEL_CURVE_SATURATED,
} VEL_CURVE;

/*
 * struct key:
 *
 * This struct represents a single button for an abstract MIDI keyboard.
 * One key struct is created for every two GPIO ports/numbers passed via
 * the `gpio_mapping' kernel parameter.
 *
 * @KEY_STATE: The state is used to build a state machine like logic for
 *   every button; something like INACTIVE->TOUCHED->PRESSED->INACTIVE...
 * @gpios: The two GPIOs which are used to build every button in hardware.
 * @irqs: The IRQ numbers for the corresponding GPIOs.
 * @hit_time: Time (in ns) when the button was hit/pressed.
 * @timer_started: This is used to mitigate the jittering on every GPIO port.
 * @hrtimers: Those are used to set a timeout for sending MIDI events.
 * @note: The corresponding MIDI note.
 * @last_velocity: The velocity (= strength) of the button hit.
 */
struct key {
	KEY_STATE state;
	struct gpio gpios[2];
	unsigned int irqs[2];
	ktime_t hit_time;
	bool timer_started[2];
	struct hrtimer hrtimers[2];
	unsigned char note;
	int last_velocity;
};

/*
 * cmidid_gpio_state:
 *
 * The state of this GPIO component of our kernel module.
 * @keys: The array of available keys for our keyboard.
 * @num_keys: The total number of available keys.
 *
 * TODO: button_active_high is currently unused. Implement IOCTL.
 */
struct cmidid_gpio_state {
	struct key *keys;
	int num_keys;
	bool button_active_high[2];
	uint32_t last_stroke_time;
	uint32_t stroke_time_min;
	uint32_t stroke_time_max;
	VEL_CURVE vel_curve;
};

int get_key_from_irq(int irq, struct key **k_ret, unsigned char *button_ret);
static unsigned char time_to_velocity(uint32_t t);
static uint32_t stime64_to_utime32(s64 stime64);

struct cmidid_gpio_state state;

uint32_t set_min_stroke_time()
{
	dbg("min stroke time set to %d\n", state.last_stroke_time);
	return state.stroke_time_max = state.last_stroke_time;
}

uint32_t set_max_stroke_time()
{
	dbg("max stroke time set to %d\n", state.last_stroke_time);
	return state.stroke_time_max = state.last_stroke_time;
}

void set_vel_curve_linear()
{
	dbg("velocity curve set to linear\n");
	state.vel_curve = VEL_CURVE_LINEAR;
}

void set_vel_curve_concave()
{
	dbg("velocity curve set to concave\n");
	state.vel_curve = VEL_CURVE_CONCAVE;
}

void set_vel_curve_convex()
{
	dbg("velocity curve set to convex\n");
	state.vel_curve = VEL_CURVE_CONVEX;
}

void set_vel_curve_saturated()
{
	dbg("velocity curve set to saturated\n");
	state.vel_curve = VEL_CURVE_SATURATED;
}

/*
 * handle_button_event: Changes the state of the given key according to the
 * previous stateand the state of the given button.
 *
 * @k: The key which is associated with the current button event.
 * @button: The id of the button. Can be START_BUTTON or END_BUTTON.
 * @active: true if the button was pressed.
 */
static void handle_button_event(struct key *k, unsigned char button,
				bool active)
{
	unsigned char velocity;
	uint32_t timediff;

	switch (k->state) {
	case KEY_INACTIVE:
		if ((button == START_BUTTON) && active) {
			/* First button was hit; button not pressed completely. */
			k->hit_time = ktime_get();
			k->state = KEY_TOUCHED;
		} else if ((button == START_BUTTON) && !active) {
			/* First buttons was release => key was released. */
			note_off(k->note);
		}
		break;
	case KEY_TOUCHED:
		if ((button == START_BUTTON) && !active) {
			note_off(k->note);
			k->state = KEY_INACTIVE;
		} else if ((button == END_BUTTON) && active) {
			timediff =
			    stime64_to_utime32(ktime_sub
					       (ktime_get(), k->hit_time).tv64);

			state.last_stroke_time = timediff;

			velocity = time_to_velocity(timediff);
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
			note_off(k->note);
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

static uint32_t stime64_to_utime32(s64 stime64)
{
	stime64 = stime64 >> 10;
	return (uint32_t) stime64;
}

static unsigned char time_to_velocity(uint32_t t)
{
	uint32_t a, b, c, t_sat;

	switch (state.vel_curve) {
	case VEL_CURVE_LINEAR:
		return 127 * (state.stroke_time_max -
			      t) / (state.stroke_time_max -
				    state.stroke_time_min);
	case VEL_CURVE_CONCAVE:
		c = 255;
		a = (-127 * c + c * c) * (state.stroke_time_max -
					  state.stroke_time_min) / 127;
		b = (c * state.stroke_time_max + 127 * state.stroke_time_min -
		     c * state.stroke_time_min);
		return a / (t - b) + c;
	case VEL_CURVE_CONVEX:
		if (t < state.stroke_time_min) {
			return 127;
		} else {
			c = -40;
			a = (-127 * c + c * c) * (state.stroke_time_max -
						  state.stroke_time_min) / 127;
			b = (c * state.stroke_time_max +
			     127 * state.stroke_time_min -
			     c * state.stroke_time_min);
			return a / (t - b) + c;
		}
	case VEL_CURVE_SATURATED:
		c = 140;
		t_sat =
		    state.stroke_time_min + (state.stroke_time_max -
					     state.stroke_time_min) / 2;
		a = (-127 * c + c * c) * (state.stroke_time_max - t_sat) / 127;
		b = (c * state.stroke_time_max + 127 * t_sat - c * t_sat);
		return a / (t - b) + c;
	}

	return 0;
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

static struct key *get_key_from_timer(struct hrtimer *timer,
				      unsigned char *index)
{
	int i;

	for (i = 0; i < state.num_keys; i++) {
		if (&state.keys[i].hrtimers[START_BUTTON] == timer) {
			*index = START_BUTTON;
			return &state.keys[i];
		}
		if (&state.keys[i].hrtimers[END_BUTTON] == timer) {
			*index = END_BUTTON;
			return &state.keys[i];
		}
	}

	return NULL;
}

enum hrtimer_restart timer_irq(struct hrtimer *timer)
{
	struct key *k;
	int gpio_active;	// gpio_value_start, gpio_value_end;
	unsigned char button;

	/* The following line removes an annoying warning :) */
	k = NULL;
	k = get_key_from_timer(timer, &button);

	if (k == NULL) {
		err("htimer not found \n");
		return HRTIMER_NORESTART;
	}

	gpio_active = gpio_get_value(k->gpios[button].gpio);

	k->timer_started[button] = false;

	info("Timer Button GPIO %d detected as %hhd index: %d\n",
	     k->gpios[button].gpio, gpio_active, button);
	handle_button_event(k, button, !gpio_active);

	return HRTIMER_NORESTART;
}

/*
 * The interrupt handler for every GPIO interrupt.
 */
static irqreturn_t irq_handler(int irq, void *dev_id)
{
	struct key *k;
	unsigned char index;
	ktime_t diff;
	int gpio_value_start, gpio_value_end, res;

	ktime_t time = ktime_get();
	index = 0;
	k = NULL;
	get_key_from_irq(irq, &k, &index);

	gpio_value_start = gpio_get_value(k->gpios[START_BUTTON].gpio);
	gpio_value_end = gpio_get_value(k->gpios[END_BUTTON].gpio);

	info("Interrupt handler called %d: gpio value [%d, %d]. (%lld ns)\n",
	     irq, gpio_value_start, gpio_value_end, time.tv64);

	//TODO: into global space :
	diff = ktime_set(0, jitter_res_time);

	if (k != NULL && !k->timer_started[index]) {
		res =
		    hrtimer_start(&k->hrtimers[index], diff, HRTIMER_MODE_REL);
		dbg("hrtimer_start res: :%d index: %d\n", res, index);
		k->timer_started[index] = true;
	} else {
		dbg("Ignore jitter for k: %p button %d timer started: %d.", k,
		    index, k->timer_started[index]);
	}
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

	state.last_stroke_time = 100000;
	state.stroke_time_min = 1000;
	state.stroke_time_max = 1000000;

	state.vel_curve = VEL_CURVE_LINEAR;

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
		k->timer_started[START_BUTTON] = false;
		k->timer_started[END_BUTTON] = false;

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
