#include <linux/kernel.h>

#include "midi.h"
#include "clock.h"
#include "alsa.h"

/**
 * @brief Initialize a MIDIDriver instance.
 * @public @memberof MIDIDriver
 * @param name The name to identify the driver.
 * @param rate The sampling rate to use.
 */
void MIDIDriverInit(struct MIDIDriver *driver, char *name,
		    MIDISamplingRate rate, void *drv)
{
	// MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );

	pr_debug("init MIDIDriver\n");

	driver->refs = 1;
	// driver->port  = MIDIPortCreate( name, MIDI_PORT_IN | MIDI_PORT_OUT,
	// driver, &_port_receive );
	driver->port = ALSARegisterClient(drv); // ALSARegisterClient();
	driver->clock = MIDIClockProvide(rate);

	// driver->send    = NULL;
	// driver->destroy = NULL;
}

/**
 * @brief Destroy a MIDIDriver instance.
 * Free all resources occupied by the driver and release all referenced objects.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverDestroy(struct MIDIDriver *driver)
{
	// MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
	/*if( driver->destroy != NULL ) {
	  (*driver->destroy)( driver );
	}*/
	if (driver->clock != NULL) {
		MIDIClockRelease(driver->clock);
	}
	ALSADeleteClient(driver->port);
	// MIDIPortInvalidate( driver->port );
	// MIDIPortRelease( driver->port );
}

/**
 * @brief Retain a MIDIDriver instance.
 * Increment the reference counter of a driver so that it won't be destroyed.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverRetain(struct MIDIDriver *driver)
{
	// MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
	driver->refs++;
}

/**
 * @brief Release a MIDIDriver instance.
 * Decrement the reference counter of a driver. If the reference count
 * reached zero, destroy the driver.
 * @public @memberof MIDIDriver
 * @param driver The driver.
 */
void MIDIDriverRelease(struct MIDIDriver *driver)
{
	// MIDIPrecondReturn( driver != NULL, EFAULT, (void)0 );
	if (!--driver->refs) {
		MIDIDriverDestroy(driver);
	}
}