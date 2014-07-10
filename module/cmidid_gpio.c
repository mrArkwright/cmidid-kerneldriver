#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>

#include "cmidid_util.h"
#include "cmidid_gpio.h"
#include "cmidid_midi.h"

/*
 * GPIOs requested via kernel parameter.
 * This is a simple list of gpio ids with an corresponding pitch.
 * The format for passing the values is:
 * gpio_mapping=gpio1a,gpio1b,note1,gpio2a,gpio2b,note2
 */
static int gpio_mapping[MAX_KEYS];
static int gpio_mapping_size;
module_param_array(gpio_mapping, int, &gpio_mapping_size, 0);
MODULE_PARM_DESC(gpio_mapping,
		 "Mapping of GPIOs to Keys. Format: gpio1a, gpio1b, note1, gpio2a, ...");

/*
 * Specifies the polarity (electrical combined with logical in respect
 * to the key contruction) of the start button of each key.
 */
static bool start_button_active_high;
module_param(start_button_active_high, bool, 0);
MODULE_PARM_DESC(start_button_active_high,
		 "is the start button of each key activated on a rising edge?");

/*
 * Specifies the polarity (electrical combined with logical in respect
 * to the key contruction) of the end button of each key.
 */
static bool end_button_active_high;
module_param(end_button_active_high, bool, 0);
MODULE_PARM_DESC(end_button_active_high,
		 "is the end button of each key activated on a rising edge?");

/*
 * The minimum time difference between two subsequent button presses which
 * results in the maximum velocity (127) for the buttton press.
 */
static uint32_t stroke_time_min = 1000;
module_param(stroke_time_min, uint, 0);
MODULE_PARM_DESC(stroke_time_min, "stroke time for maximal velocity");

/*
 * The time differnence between two button hits for a single key which
 * which results in the minimum (0) velocity for the sent MIDI note.
 * Press times above this value will return the minium velocity, too.
 */
static uint32_t stroke_time_max = 1000000;
module_param(stroke_time_max, uint, 0);
MODULE_PARM_DESC(stroke_time_max, "stroke time for minimal velocity");

/*
 * The time offset before every MIDI event is sent.
 * Delaying the sending of MIDI events while ignoring subsequent key
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
 * Somwhat like this:
 *
 *   |  |
 *   v  v  v
 *  +--------------+
 *  | keyboard key | The key is fixed on this side :)
 *  +..............+=========
 *
 *   .---.    .---.
 *    end     start
 *
 * Example usage: state.keys[i].gpios[START_BUTTON].gpio ...
 */
#define START_BUTTON 0
#define END_BUTTON 1

/*
 * Possible states for every key of our MIDI keyboard.
 * These values are used in `handle_button_event'.
 *
 * @KEY_INACTIVE: The key is not touched or pressed.
 * @KEY_TOUCHED: The first button of the key is hit/pressed down.
 * @KEY_PRESSED: The second button is hit, so the key is completely pressed.
 */
typedef enum {
	KEY_INACTIVE,
	KEY_TOUCHED,
	KEY_PRESSED
} KEY_STATE;

/*
 * VEL_CURVE: The types of interpolation curves used to calculate the
 * velocity for every noteon MIDI event.
 */
typedef enum {
	VEL_CURVE_LINEAR,
	VEL_CURVE_CONCAVE,
	VEL_CURVE_CONVEX,
	VEL_CURVE_SATURATED,
} VEL_CURVE;

/*
 * struct key:
 *
 * This struct represents a single key for an abstract MIDI keyboard.
 * Each of those keys is associated with two buttons/triggers which are
 * connected to two different GPIO ports of the machine.
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
 * TODO: button_active_high is currently unused. Implement module params.
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
 * previous state and the state of the given button.
 *
 * @k: The key which is associated with the current button event.
 * @button: The id of the button. Can be START_BUTTON or END_BUTTON.
 * @active: true if the button was pressed, false if the button was released.
 */
static void handle_button_event(struct key *k, unsigned char button,
				bool active)
{
	unsigned char velocity;
	uint32_t timediff;

	/* Switch the last state of the current key. */
	switch (k->state) {
	case KEY_INACTIVE:
		/* The key was inactive (not pressed in any way) previously. */
		if ((button == START_BUTTON) && active) {
			/* First button was hit -> button not pressed completely. */
			k->hit_time = ktime_get();
			k->state = KEY_TOUCHED;
		} else if ((button == START_BUTTON) && !active) {
			/* First buttons was release -> key was released. */
			cmidid_note_off(k->note);
		}
		break;
	case KEY_TOUCHED:
		/* Only the first button of the key was pressed previously. */
		if ((button == START_BUTTON) && !active) {
			/* The first button is released -> not pressed. */
			cmidid_note_off(k->note);
			k->state = KEY_INACTIVE;
		} else if ((button == END_BUTTON) && active) {
			/* The second button is hit -> pressed completely. */
			timediff =
			    stime64_to_utime32(ktime_sub
					       (ktime_get(), k->hit_time).tv64);

			state.last_stroke_time = timediff;

			velocity = time_to_velocity(timediff);
			cmidid_note_on(k->note, velocity);

			k->last_velocity = velocity;
			k->state = KEY_PRESSED;
		}
		break;
	case KEY_PRESSED:
		/* The key was fully pressed down previously. */
		if ((button == START_BUTTON) && !active) {
			/* The first button was released -> not pressed.
			 * Note: This shouldn't happen (?) for a real key.
			 */
			cmidid_note_off(k->note);
			k->state = KEY_INACTIVE;
		} else if ((button == END_BUTTON) && active) {
			/* The second button was hit again. */
			cmidid_note_off(k->note);
			cmidid_note_on(k->note, k->last_velocity);
		}
		break;
	default:
		cmidid_note_off(k->note);
		k->state = KEY_INACTIVE;
	}

	dbg("key state: %d, button: %d, active: %d, note: %d\n", k->state,
	    button, active, k->note);
}

/*
 * time_to_velocity: Returns a MIDI velocity value between 0 and 127
 * for a given key hit time (in ns).
 *
 * @stime64: signed 64bit integer representing a time value in nanoseconds.
 *
 * Return: the corresponding velocity.
 */
static uint32_t stime64_to_utime32(s64 stime64)
{
	stime64 = stime64 >> 10;
	return (uint32_t) stime64;
}

/*
 * time_to_velocity: Returns a MIDI velocity value between (usually 0 and 127)
 * for a given time in nanoseconds. This should probably be the time delay
 * between two successive button hits for a single key.
 *
 * @t: time value in ns
 *
 * Return: The calculated velocity. Values < 0 are results of failed 
 *         calculations.
 */
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

/*
 * get_key_from_irq: Sets the key struct and the button index for a given irq.
 *
 * @irq: The IRQ value which is used to find the enclosing key struct.
 * @key_ret: This pointer will be set to the struct containing irq.
 * @button_ret: The index of the button/irq will be written into this char.
 *
 * Return: 0 if a struct could be found, -1 otherwise.
 */
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

/*
 * get_key_from_timer: Helper function to get the enclosing key struct
 * and the button index for a given hrtimer pointer.
 *
 * @timer: Pointer which is used to find the enclosing key struct.
 * @index: The index of the corresponding button is written to this location.
 *
 * Return: The pointer to the struct which contains `timer' or NULL if no
 * such struct could be found.
 */
static struct key *get_key_from_timer(struct hrtimer *timer,
				      unsigned char *index)
{
	struct key *k;

	for (k = state.keys; k < state.keys + state.num_keys; k++) {
		if (&k->hrtimers[START_BUTTON] == timer) {
			*index = START_BUTTON;
			return k;
		}
		if (&k->hrtimers[END_BUTTON] == timer) {
			*index = END_BUTTON;
			return k;
		}
	}

	return NULL;
}

/*
 * timer_irq: The interrupt handler routine reads the GPIO values to
 * determine the state of the corresponding button and subsequently callsx 
 * handle_button_event. This function is delayed for a fixed time after
 * the interrupt on the GPIO port is registered.
 *
 * @timer: hrtimer that uses this function as a callback
 *
 * Return: HRTIMER_NORESTART; the timer has to be reset manually.
 */
enum hrtimer_restart timer_irq(struct hrtimer *timer)
{
	struct key *k;
	int gpio_active;	// gpio_value_start, gpio_value_end;
	unsigned char button;

	/* The following line removes an annoying warning :) TODO */
	k = NULL;
	k = get_key_from_timer(timer, &button);

	if (k == NULL) {
		err("htimer not found \n");
		return HRTIMER_NORESTART;
	}

	gpio_active = gpio_get_value(k->gpios[button].gpio);

	/* Reset the flag, so that new interrupts will be registered. */
	k->timer_started[button] = false;

	dbg("Timer Button GPIO %d detected as %hhd index: %d\n",
	    k->gpios[button].gpio, gpio_active, button);
	handle_button_event(k, button,
			    !(state.button_active_high[button] ^ gpio_active));

	return HRTIMER_NORESTART;
}

/*
 * irq_handler: The interrupt handler for every GPIO interrupt.
 * This function is called when a rising/falling edge is registered on the
 * GPIO port corresponding to the given IRQ number.
 *
 * Software resolution of jittering/bouncing is achieved by delaying the
 * read on the GPIO port by a fixed amount of time. Other interrupts for
 * this port are ignored during this period of time.
 *
 * @irq: The number of the IRQ which is resposible for calling this function.
 * @dev_id: I have no idea :) Look at linux/interrupt.h.
 *
 * Return: IRQ_HANDLED
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

	/* Only for debugging purposes: */
	gpio_value_start = gpio_get_value(k->gpios[START_BUTTON].gpio);
	gpio_value_end = gpio_get_value(k->gpios[END_BUTTON].gpio);

	dbg("Interrupt handler called %d: gpio value [%d, %d]. (%lld ns)\n",
	    irq, gpio_value_start, gpio_value_end, time.tv64);

	diff = ktime_set(0, jitter_res_time);

	if (k != NULL && !k->timer_started[index]) {
		/* Call the timer_irq delayed and "lock" the interrupt handler. */
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

/*
 * is_valid: Checks if a given GPIO number is already used for a key struct
 * or if the GPIO number is greater than the number of GPIOs available for
 * the current machine.
 *
 * @gpio: The GPIO number to be checked.
 *
 * Return: true is the number is a valid GPIO; false otherwise
 */
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

/*
 * gpio_init: Initialization routine for the GPIO component of the CMIDID
 * kernel driver. This will be called by cmidid_init.
 *
 * Return: A Linux error code.
 */
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
	state.stroke_time_min = stroke_time_min;
	state.stroke_time_max = stroke_time_max;

	state.vel_curve = VEL_CURVE_LINEAR;

	state.button_active_high[START_BUTTON] = start_button_active_high;
	state.button_active_high[END_BUTTON] = end_button_active_high;

	/* Initialize the state and key structs. */
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

		/* Make sure that the timer init is called before 
		 * requesting the irqs. That's because request_irq calls
		 * the `irq_handler' function immediately (which uses the
		 * hrtimer).
		 */
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

	/* Free in reverse order. */
	for (--i; i > 0; --i) {
		gpio_free_array(state.keys[i].gpios, 2);
		hrtimer_cancel(&state.keys[i].hrtimers[END_BUTTON]);
		hrtimer_cancel(&state.keys[i].hrtimers[START_BUTTON]);
	}

	kfree(state.keys);

	return err;
}

/*
 * gpio_exit: Exit/cleanup routine called by the __exit routine of the
 * actual module. Definitely, free everything here.
 */
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
