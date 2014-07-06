
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include "clock.h"

#include <linux/slab.h>

static void _init_clock_kernel(struct MIDIClock *clock)
{
	clock->numer = 1;
	clock->denom = NSEC_PER_SEC;
}

static unsigned long long _timestamp_kernel(void)
{
	ktime_t t = ktime_get();
	return (t.tv64);
}

static struct
{
	void (*init)(struct MIDIClock *);
	unsigned long long (*timestamp)(void);
} _midi_clock[] = { { &_init_clock_kernel, &_timestamp_kernel } };

/**
 * @brief The global clock.
 * @private @memberof MIDIClock
 */
static struct MIDIClock *_midi_global_clock = NULL;

/**
 * @brief Normalize a fraction.
 * Try to divide the numerator and denominator by a small list of known primes.
 * Chances are the fraction is normalized after that.
 * @private @memberof MIDIClock
 * @param numer The numerator.
 * @param denom The denominator.
 */
static void _normalize_frac(unsigned long long *numer,
			    unsigned long long *denom)
{
	static int prime[] = { 7, 5, 3, 2, 1 };
	int i;
	for (i = 0; prime[i] > 1; i++) {
		while ((*numer % prime[i] == 0) && (*denom % prime[i] == 0)) {
			*numer /= prime[i];
			*denom /= prime[i];
		}
	}
}

/**
 * @brief Divide a fraction by a scalar.
 * Factorize the scalar to integers and apply them to the fraction.
 * @private @memberof MIDIClock
 * @param numer The numerator.
 * @param denom The denominator.
 * @param fac   The factor to divide by.
 */
static void _divide_frac(unsigned long long *numer, unsigned long long *denom,
			 unsigned long long fac)
{
	static int prime[] = { 7, 5, 3, 2, 1 };
	int i;
	for (i = 0; prime[i] > 1; i++) {
		while ((fac % prime[i]) == 0) {
			if ((*numer % prime[i]) == 0) {
				*numer /= prime[i];
			} else {
				*denom *= prime[i];
			}
			fac /= prime[i];
		}
	}
	*denom *= fac;
}

/**
 * @brief Multiply a fraction with a scalar.
 * @see _divide_frac
 * @private @memberof MIDIClock
 * @param numer The numerator.
 * @param denom The denominator.
 * @param fac   The factor to multiply with.
 */
static void _multiply_frac(unsigned long long *numer, unsigned long long *denom,
			   unsigned long long fac)
{
	_divide_frac(denom, numer, fac);
}

/**
 * @brief Get the real time.
 * Use the clock's timestamp function to get an implementation-specific
 * timestamp. Then convert it to the desired rate using the internal fraction.
 * @private @memberof MIDIClock
 * @param clock The clock.
 */
static MIDITimestamp _get_real_time(struct MIDIClock *clock)
{
	return ((*_midi_clock[0].timestamp)() * clock->numer) / clock->denom;
}

/**
 * @brief Create a MIDIClock instance.
 * Allocate space and initialize a MIDIClock instance.
 * @public @memberof MIDIClock
 * @param rate The number of times the clock should tick per second.
 * @return a pointer to the created clock structure on success.
 * @return a @c NULL pointer if the clock could not created.
 */
struct MIDIClock *MIDIClockCreate(MIDISamplingRate rate)
{
	struct MIDIClock *clock = kmalloc(sizeof(struct MIDIClock), GFP_KERNEL);
	// MIDIPrecondReturn( clock != NULL, ENOMEM, NULL );

	clock->refs = 1;
	(*_midi_clock[0].init)(clock);
	if (rate == 0)
		rate = (clock->denom / clock->numer);
	_multiply_frac(&(clock->numer), &(clock->denom), rate);
	_normalize_frac(&(clock->numer), &(clock->denom));
	clock->rate = rate;
	clock->offset = -1 * _get_real_time(clock);
	return clock;
}

/**
 * @brief Provide clock with a specific timestamp rate.
 * Check if the global clock has the requested sampling rate. If the
 * rate matches, retain and return the global clock. Create a new
 * clock otherwise.
 * If the global clock still NULL, assign the newly created clock.
 * @public @memberof MIDIClock
 * @param rate The number of times the clock should tick per second.
 * @return a pointer to the created clock structure on success.
 * @return a @c NULL pointer if the clock could not created.
 */
struct MIDIClock *MIDIClockProvide(MIDISamplingRate rate)
{
	if (_midi_global_clock == NULL) {
		_midi_global_clock = MIDIClockCreate(rate);
		MIDIClockRetain(_midi_global_clock);
		return _midi_global_clock;
	} else if (_midi_global_clock->rate == rate) {
		MIDIClockRetain(_midi_global_clock);
		return _midi_global_clock;
	} else {
		return MIDIClockCreate(rate);
	}
}

/**
 * @brief Get the global clock.
 * Provide a pointer to the global clock.
 * If there is no global clock yet, create one using the default
 * sampling rate.
 * @private @memberof MIDIClock
 * @return a pointer to the global clock.
 */
static struct MIDIClock *_get_global_clock(void)
{
	if (_midi_global_clock == NULL) {
		_midi_global_clock =
		    MIDIClockCreate(MIDI_SAMPLING_RATE_DEFAULT);
	}
	return _midi_global_clock;
}

/**
 * @brief Destroy a MIDIClock instance.
 * Free all resources occupied by the clock and release all referenced objects.
 * @public @memberof MIDIClock
 * @param clock The clock.
 */
void MIDIClockDestroy(struct MIDIClock *clock)
{
	// MIDIPrecondReturn( clock != NULL, EFAULT, (void)0 );
	kfree(clock);
}

/**
 * @brief Retain a MIDIClock instance.
 * Increment the reference counter of a clock so that it won't be destroyed.
 * @public @memberof MIDIClock
 * @param clock The clock.
 */
void MIDIClockRetain(struct MIDIClock *clock)
{
	// MIDIPrecondReturn( clock != NULL, EFAULT, (void)0 );
	clock->refs++;
}

/**
 * @brief Release a MIDIClock instance.
 * Decrement the reference counter of a clock. If the reference count
 * reached zero, destroy the clock.
 * @public @memberof MIDIClock
 * @param clock The clock.
 */
void MIDIClockRelease(struct MIDIClock *clock)
{
	// MIDIPrecondReturn( clock != NULL, EFAULT, (void)0 );
	if (!--clock->refs) {
		MIDIClockDestroy(clock);
	}
}

/**
 * @brief Get the current time.
 * Get the current time.
 * @public @memberof MIDIClock
 * @param clock The clock to read (pass @c NULL for global clock)
 * @param now   The current time.
 * @retval 0 on success.
 */
int MIDIClockGetNow(struct MIDIClock *clock, MIDITimestamp *now)
{
	// MIDIPrecond( now != NULL, EINVAL );
	if (clock == NULL)
		clock = _get_global_clock();
	*now = _get_real_time(clock) + clock->offset;
	return 0;
}