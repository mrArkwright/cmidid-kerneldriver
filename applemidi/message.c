#include <linux/kernel.h>

#include "message.h"


#define VOID_BYTE( buffer, n ) ((unsigned char*)buffer)[n]

/**
 * @public @memberof MIDIMessage
 * @param message The message.
 * @param timestamp The MIDITimestamp.
 */
int MIDIMessageSetTimestamp( struct MIDIMessage * message, MIDITimestamp timestamp ) {
  //MIDIPrecond( message != NULL, EFAULT );
  message->timestamp = timestamp;
  return 0;
}

/**
 * @public @memberof MIDIMessage
 * @param message The message.
 * @param timestamp The MIDITimestamp.
 */
int MIDIMessageGetTimestamp( struct MIDIMessage * message, MIDITimestamp * timestamp ) {
  //MIDIPrecond( message != NULL, EFAULT );
  //MIDIPrecond( timestamp != NULL , EINVAL);
  *timestamp = message->timestamp;
  return 0;
}

static int _check_encode_running_status( unsigned char byte, MIDIRunningStatus * status ) {
  if( status == NULL || *status == 0 ) return 0;
  if( byte >= 0x80 && byte <= 0xef ) {
    if( byte == *status ) {
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}

static int _check_decode_running_status( unsigned char byte, MIDIRunningStatus * status ) {
  if( status == NULL || *status == 0 ) return 0;
  if( byte < 0x80 ) return 1;
  return 0;
}


static int _update_running_status( struct MIDIMessageData * data, MIDIRunningStatus * status ) {
  if( status == NULL ) return 0;
  if( data->bytes[0] >= 0x80 && data->bytes[0] <= 0xef ) {
    *status = data->bytes[0];
  } else if( data->bytes[0] >= 0xf0 && data->bytes[0] <= 0xf7 ) {
    *status = 0;
  } else if( data->bytes[0] >= 0xf8 && data->bytes[0] <= 0xff ) {
    /* real time message */
  } else {
    return 1;
  }
  return 0;
}


static int _size_three_bytes( struct MIDIMessageData * data, size_t * size ) {
  if( data == NULL || size == NULL ) return 1;
  *size = 3;
  return 0;
}

static int _encode_three_bytes( struct MIDIMessageData * data, MIDIRunningStatus * status, size_t size, void * buffer, size_t * written ) {
  //MIDIAssert( data != NULL && buffer != NULL );
  if( _check_encode_running_status( data->bytes[0], status ) ) {
    if( size < 2 ) return 1;
    VOID_BYTE(buffer,0) = data->bytes[1];
    VOID_BYTE(buffer,1) = data->bytes[2];
    if( written != NULL ) *written = 2;
  } else if( _update_running_status( data, status ) == 0) {
    if( size < 3 ) return 1;
    VOID_BYTE(buffer,0) = data->bytes[0];
    VOID_BYTE(buffer,1) = data->bytes[1];
    VOID_BYTE(buffer,2) = data->bytes[2];
    if( written != NULL ) *written = 3;
  }
  return 0;
}

static int _decode_three_bytes( struct MIDIMessageData * data, MIDIRunningStatus * status, size_t size, void * buffer, size_t * read ) {
  //MIDIAssert( data != NULL && buffer != NULL );
  if( _check_decode_running_status( VOID_BYTE(buffer,0), status ) ) {
    if( size < 2 ) return 1;
    data->bytes[0] = *status;
    data->bytes[1] = VOID_BYTE(buffer,0);
    data->bytes[2] = VOID_BYTE(buffer,1);
    if( read != NULL ) *read = 2;
  } else {
    if( size < 3 ) return 1;
    data->bytes[0] = VOID_BYTE(buffer,0);
    data->bytes[1] = VOID_BYTE(buffer,1);
    data->bytes[2] = VOID_BYTE(buffer,2);
    if( read != NULL ) *read = 3;
  }
  return _update_running_status( data, status );
}

static int _test_note_off_on( void * buffer ) {
  return (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_NOTE_OFF<<4)
      || (VOID_BYTE(buffer,0) & 0xf0) == (MIDI_STATUS_NOTE_ON<<4);
}


#define PROPERTY_CASE_BASE(flag,type) \
    case flag: \
      if( size != sizeof(type) ) return 1

#define PROPERTY_CASE_SET(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & ( (flag==MIDI_STATUS) ? 0xff : 0x7f ) ) != *((type*)value) ) return 1; \
      field = *((type*)value); \
      return 0

#define PROPERTY_CASE_SET_H(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & ( (flag==MIDI_STATUS) ? 0x0f : 0x07 ) ) != *((type*)value) ) return 1; \
      field = MIDI_NIBBLE_VALUE( *((type*)value), MIDI_LOW_NIBBLE(field) ); \
      return 0

#define PROPERTY_CASE_SET_L(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      if( ( *((type*)value) & 0x0f ) != *((type*)value) ) return 1; \
      field = MIDI_NIBBLE_VALUE( MIDI_HIGH_NIBBLE(field), *((type*)value) ); \
      return 0

#define PROPERTY_CASE_GET(flag,type,field) \
    PROPERTY_CASE_BASE(flag,type); \
      *((type*)value) = (type) field; \
      return 0

#define PROPERTY_CASE_GET_H(flag,type,field) PROPERTY_CASE_GET(flag,type,MIDI_HIGH_NIBBLE(field))
#define PROPERTY_CASE_GET_L(flag,type,field) PROPERTY_CASE_GET(flag,type,MIDI_LOW_NIBBLE(field))
      
#define PROPERTY_DEFAULT \
    default: \
      return 1

/**
 * Set properties of note on/off messages.
 * @param data     The message data object to write to.
 * @param property The property to set.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied to the message property.
 * @retval 0 on success.
 * @retval 1 if the property was not set.
 */
static int _set_note_off_on( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  unsigned char * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_SET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_SET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_SET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_SET(MIDI_VELOCITY,MIDIVelocity,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

/**
 * Get properties of note on/off messages.
 * @param data     The message data object to read from.
 * @param property The property to get.
 * @param size     The size of the memory object pointed to by @c value.
 * @param value    A pointer to the memory object who's contents shall be
 *                 copied from the message property.
 * @retval 0 on success.
 * @retval 1 if the value was not set.
 */
static int _get_note_off_on( struct MIDIMessageData * data, MIDIProperty property, size_t size, void * value ) {
  unsigned char * m = &(data->bytes[0]);
  if( size == 0 || value == NULL ) return 1;
  switch( property ) {
    PROPERTY_CASE_GET_H(MIDI_STATUS,MIDIStatus,m[0]);
    PROPERTY_CASE_GET_L(MIDI_CHANNEL,MIDIChannel,m[0]);
    PROPERTY_CASE_GET(MIDI_KEY,MIDIKey,m[1]);
    PROPERTY_CASE_GET(MIDI_VELOCITY,MIDIVelocity,m[2]);
    PROPERTY_DEFAULT;
  }
  return 1;
}

static struct MIDIMessageFormat _note_off_on = {
  &_test_note_off_on,
  &_size_three_bytes,
  &_set_note_off_on,
  &_get_note_off_on,
  &_encode_three_bytes,
  &_decode_three_bytes
};


#define N_ELEM(a) (sizeof(a) / sizeof(a[0]))

/**
 * @brief Detect the format of message stored in a buffer.
 * Determine the message format used in a stream of bytes.
 * @public @memberof MIDIMessageFormat
 * @param buffer The message as it would appear on a MIDI cable.
 * @return a pointer to the correct message format if the format could be detected.
 * @return a NULL pointer if the format could not be detected.
 */
struct MIDIMessageFormat * MIDIMessageFormatDetect( void * buffer ) {
  static struct MIDIMessageFormat * formats[] = {
    &_note_off_on
    /*,&_polyphonic_key_pressure,
    &_control_change,
    &_program_change,
    &_channel_pressure,
    &_pitch_wheel_change,
    &_system_exclusive,
    &_time_code_quarter_frame,
    &_song_position_pointer,
    &_song_select,
    &_tune_request,
    &_real_time*/
  };
  int i;
  //MIDIPrecondReturn( buffer != NULL, EFAULT, NULL );
  for( i=0; i<N_ELEM(formats); i++ ) {
    //MIDIAssert( formats[i] != NULL && formats[i]->test != NULL );
    if( (formats[i]->test)( buffer ) ) {
		printk("detected format %i\n",i);
      return formats[i];
    }
  }
  return NULL;
}


/**
 * @brief Encode messages with running status byte.
 * Encode message data objects into a buffer.
 * @public @memberof MIDIMessageFormat
 * @param format   The message format.
 * @param data     The message data object to read from.
 * @param status   The running status inherited from previous messages.
 * @param size     The size of the memory pointed to by @c buffer.
 * @param buffer   The buffer to encode the message into.
 * @param written  The number of bytes that were written to the @c buffer.
 * @retval 0 on success.
 * @retval 1 if the message could not be encoded.
 */
int MIDIMessageFormatEncodeRunningStatus( struct MIDIMessageFormat * format, struct MIDIMessageData * data, MIDIRunningStatus * status, size_t size, void * buffer, size_t * written ) {
  //MIDIPrecond( format != NULL, EFAULT );
  //MIDIPrecond( size > 0 && buffer != NULL, EINVAL );
  //MIDIAssert( format->encode != NULL );
	if(format != NULL)
	{
		printk("MSG Enc running status format !NULL");
		if(format->encode != NULL)
		{
			printk("MSG Enc running status format->enc !NULL");
		  return (format->encode)( data, status, size, buffer, written );
	  	}
		else
		{
			printk("MSG Enc running status format->end NULL");
			return 1;
		}
	}
	else
	{
		printk("MSG Enc running status format NULL");
		return 1;
	}
}


/**
 * @brief Encode messages with running status.
 * Encode message objects into a buffer. Use the running status and update it if necessary.
 * @public @memberof MIDIMessageFormat
 * @param message  The message.
 * @param status   A pointer to the running status.
 * @param size     The size of the memory pointed to by @c buffer.
 * @param buffer   The buffer to decode the message from.
 * @param written  The number of bytes that were actually written.
 * @retval 0 on success.
 * @retval 1 if the message could not be encoded.
 */
int MIDIMessageEncodeRunningStatus( struct MIDIMessage * message, MIDIRunningStatus * status,
                                    size_t size, unsigned char * buffer, size_t * written ) {
  //MIDIPrecond( message != NULL, EFAULT );
  //MIDIPrecond( size > 0 && buffer != NULL , EINVAL);
  return MIDIMessageFormatEncodeRunningStatus( message->format, &(message->data), status, size, buffer, written );
}