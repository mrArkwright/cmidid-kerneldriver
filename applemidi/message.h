#ifndef MESSAGE_H
#define MESSAGE_H

#include <linux/types.h>

#include "midi.h"

#define MIDI_MESSAGE_DATA_BYTES 4

typedef unsigned short MIDIProperty;
typedef unsigned char MIDIRunningStatus;
typedef unsigned int MIDIVarLen;

typedef unsigned char MIDIStatus;
typedef char MIDIChannel;
typedef char MIDIKey;
typedef char MIDIVelocity;
typedef char MIDIPressure;
typedef char MIDIControl;
typedef short MIDIControlParameter;
typedef char MIDIProgram;
typedef short MIDIManufacturerId;

struct MIDIMessageData
{
	unsigned char bytes[MIDI_MESSAGE_DATA_BYTES];
	size_t size;
	void *data;
};

/**
 * @ingroup MIDI
 * @struct MIDIMessageFormat message_format.h
 * Message format descriptor.
 * Contains pointers to functions that access messages of a certain type.
 * @see MIDIMessageData
 */
struct MIDIMessageFormat
{
	/**
	 * @privatesection
	 * @cond INTERNALS
	 */
	int (*test)(void *buffer);
	int (*size)(struct MIDIMessageData *data, size_t *size);
	int (*set)(struct MIDIMessageData *data, MIDIProperty property,
		   size_t size, void *value);
	int (*get)(struct MIDIMessageData *data, MIDIProperty property,
		   size_t size, void *value);
	int (*encode)(struct MIDIMessageData *data, MIDIRunningStatus *status,
		      size_t size, void *buffer, size_t *written);
	int (*decode)(struct MIDIMessageData *data, MIDIRunningStatus *status,
		      size_t size, void *buffer, size_t *read);
	/** @endcond */
};

/**
 * @ingroup MIDI
 * @struct MIDIMessage message.h
 * @brief Structure of MIDI message object.
 */
struct MIDIMessage
{
	/**
	 * @privatesection
	 * @cond INTERNALS
	 */
	int refs;
	struct MIDIMessageFormat *format;
	struct MIDIMessageData data;
	MIDITimestamp timestamp;
	/** @endcond */
};

struct MIDIMessageList
{
	/*size_t refs;
	  size_t length;*/
	struct MIDIMessage *message;
	struct MIDIMessageList *next;
};

int MIDIMessageSetTimestamp(struct MIDIMessage *message,
			    MIDITimestamp timestamp);
int MIDIMessageGetTimestamp(struct MIDIMessage *message,
			    MIDITimestamp *timestamp);

int MIDIMessageEncodeRunningStatus(struct MIDIMessage *message,
				   MIDIRunningStatus *status, size_t size,
				   unsigned char *buffer, size_t *written);

struct MIDIMessageFormat *MIDIMessageFormatDetect(void *);

#endif