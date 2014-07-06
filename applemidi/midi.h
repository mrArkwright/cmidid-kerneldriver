#ifndef MIDI_H
#define MIDI_H

#include <linux/types.h>

#include "clock.h"

typedef long long MIDITimestamp;

#define APPLEMIDI_CLOCK_RATE 10000

/**
 * Note Off event.
 * This message is sent when a note is released (ended).
 */
#define MIDI_STATUS_NOTE_OFF 0x8

/**
 * Note On event.
 * This message is sent when a note is depressed (start).
 */
#define MIDI_STATUS_NOTE_ON 0x9

/**
 * @name MIDI property enumeration values
 * @addtogroup MIDI
 * @see MIDIProperty
 * MIDIMessage property type enumeration.
 * @{
 */
#define MIDI_STATUS 0x00
#define MIDI_CHANNEL 0x01
#define MIDI_KEY 0x02
#define MIDI_VELOCITY 0x03
#define MIDI_PRESSURE 0x04
#define MIDI_CONTROL 0x05
#define MIDI_VALUE 0x06
#define MIDI_PROGRAM 0x07
#define MIDI_VALUE_LSB 0x08
#define MIDI_VALUE_MSB 0x09
#define MIDI_MANUFACTURER_ID 0x0a
#define MIDI_SYSEX_DATA 0x0b
#define MIDI_SYSEX_SIZE 0x0c
#define MIDI_SYSEX_FRAGMENT 0x0d
#define MIDI_TIME_CODE_TYPE 0x0e
#define MIDI_NOTHING 0xff
/** @} */

/**
 * @ingroup MIDI
 * Create a status byte with a channel number
 */
#define MIDI_NIBBLE_VALUE(h, l) (((h) << 4) | ((l) & 0xf))

/**
 * @ingroup MIDI
 * Access the high nibble of a byte.
 */
#define MIDI_HIGH_NIBBLE(b) (((b) >> 4) & 0xf)

/**
 * @ingroup MIDI
 * Access the low nibble of a byte.
 */
#define MIDI_LOW_NIBBLE(b) ((b) & 0xf)

/**
 * @ingroup MIDI
 * Compose a MIDILongValue from two MIDIValues
 */
#define MIDI_LONG_VALUE(m, l) (((m & 0x7f) << 7) | (l & 0x7f))

/**
 * @ingroup MIDI
 * Access the lowest 7 bits of a MIDILongValue
 */
#define MIDI_LSB(v) (v) & 0x7f

/**
 * @ingroup MIDI
 * Access the higher 7 bits of a MIDILongValue
 */
#define MIDI_MSB(v) ((v) >> 7) & 0x7f

/**
 * @ingroup MIDI
 * Mark a manufacturer ID as "extended".
 * Extended manufacturer IDs use 2 bytes + one empty byte.
 */
#define MIDI_MANUFACTURER_ID_EXTENDED(v) v | 0x80

/**
 * @ingroup MIDI
 * Interpret a MIDIValue as MIDIBoolean
 */
#define MIDI_BOOL(v) (((v) >= 64) ? MIDI_ON : MIDI_OFF)

struct MIDIDriver
{
	size_t refs;
	// struct MIDIPort * port;
	struct ALSADriver *port;
	struct MIDIClock *clock;
	// int (*send)( void * driver, struct MIDIMessage * message );
	// void (*destroy)( void * driver );
};

void MIDIDriverInit(struct MIDIDriver *driver, char *name,
		    MIDISamplingRate rate, void *drv);

void MIDIDriverRelease(struct MIDIDriver *driver);

#endif