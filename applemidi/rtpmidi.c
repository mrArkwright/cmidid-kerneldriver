
#include "rtp.h"
#include "message.h"

struct RTPMIDIInfo {
	unsigned char journal;
	unsigned char zero;
	unsigned char phantom;
	int len;
};

struct RTPMIDISession {
/**
 * @privatesection
 * @cond INTERNALS
 */
	size_t refs;
	struct RTPMIDIInfo midi_info;
	struct RTPPacketInfo rtp_info;
	struct RTPSession *rtp_session;

	size_t size;
	void *buffer;
/** @endcond */
};

/**
 * @brief Destroy an RTPMIDISession instance.
 * Free all resources occupied by the session and release the
 * RTP session.
 * @public @memberof RTPMIDISession
 * @param session The session.
 */
void RTPMIDISessionDestroy(struct RTPMIDISession *session)
{
	RTPSessionRelease(session->rtp_session);
	if (session->size > 0 && session->buffer != NULL) {
		kfree(session->buffer);
	}
	kfree(session);
}

/**
 * @brief Retain an RTPMIDISession instance.
 * Increment the reference counter of a session so that it won't be destroyed.
 * @public @memberof RTPMIDISession 
 * @param session The session.
 */
void RTPMIDISessionRetain(struct RTPMIDISession *session)
{
	session->refs++;
}

/**
 * @brief Create an RTPMIDISession instance.
 * Allocate space and initialize an RTPMIDISession instance.
 * @public @memberof RTPMIDISession
 * @param rtp_session The RTP session to use for transmission.
 * @return a pointer to the created structure on success.
 * @return a @c NULL pointer if the session could not created.
 */
struct RTPMIDISession *RTPMIDISessionCreate(struct RTPSession *rtp_session)
{
	struct RTPMIDISession *session =
	    kmalloc(sizeof(struct RTPMIDISession), GFP_KERNEL);
	if (session == NULL)
		return NULL;

	session->refs = 1;
	session->rtp_session = rtp_session;
	RTPSessionRetain(rtp_session);

	session->midi_info.journal = 0;
	session->midi_info.zero = 0;
	session->midi_info.phantom = 0;
	session->midi_info.len = 0;

	session->size = 512;
	session->buffer = kmalloc(session->size, GFP_KERNEL);
	if (session->buffer == NULL) {
		session->size = 0;
	}
	return session;
}

/**
 * @brief Release an RTPMIDISession instance.
 * Decrement the reference counter of a session. If the reference count
 * reached zero, destroy the session.
 * @public @memberof RTPMIDISession 
 * @param session The session.
 */

void RTPMIDISessionRelease(struct RTPMIDISession *session)
{
	if (!--session->refs) {
		RTPMIDISessionDestroy(session);
	}
}


/**
 * @brief Write a number encoding it with a variable number of bytes.
 * @param value   The location to read the number from.
 * @param size    The number of available bytes in the buffer.
 * @param buffer  The buffer to write to.
 * @param written The location to store the number of written bytes in.
 *               (may be @c NULL)
 * @retval 0 on success.
 * @retval >0 if the number could not be read.
 */
int MIDIUtilWriteVarLen( MIDIVarLen * value, size_t size, unsigned char * buffer, size_t * written ) {
  MIDIVarLen v = 0;
  unsigned char tmp[4] = { 0x80, 0x80, 0x80, 0x00 };
  size_t p = 0, q = 0;
  //MIDIPrecond( buffer != NULL && value != NULL, EINVAL );
  //MIDIPrecond( size > 0, EINVAL );
  
  pr_debug("RTPMIDI write varlen\n");

  v = *value & 0x0fffffff;
  do {
    tmp[3-p] |= v & 0x7f;
    v >>= 7;
    p++;
  } while( v != 0 && p<4 );
  if( p>size ) return 1;
  q = 4-p;
  for( p=0; q+p<4; p++ ) {
    buffer[p] = tmp[q+p];
  }
  if( written != NULL ) *written = p;
  return 0;
}

static void _advance_buffer( size_t * size, void ** buffer, size_t bytes ) {
	pr_debug("RTPMIDI advance buffer\n");
  *size   -= bytes;
  *buffer += bytes;
}

static int _rtpmidi_encode_header( struct RTPMIDIInfo * info, size_t size, void * data, size_t * written ) {
  unsigned char * buffer = data;
  
  pr_debug("RTPMIDI encoding header\n");
  
  if( info->len > 0x0fff ) return 1;
  if( info->len > 0x0f ) {
    if( size<2 ) return 1;
    buffer[0] = 0x80
              | (info->journal ? 0x40 : 0 )
              | (info->zero    ? 0x20 : 0 )
              | (info->phantom ? 0x10 : 0 )
              | ( (info->len >> 8) & 0x0f );
    buffer[1] = info->len & 0xff;
    *written = 2;
  } else {
    if( size<1 ) return 1;
    buffer[0] = (info->journal ? 0x40 : 0 )
              | (info->zero    ? 0x20 : 0 )
              | (info->phantom ? 0x10 : 0 )
              | (info->len     & 0x0f );
    *written = 1;
  }
  return 0;
}

static int _rtpmidi_encode_messages( struct RTPMIDIInfo * info, MIDITimestamp timestamp, struct MIDIMessageList * messages, size_t size, void * data, size_t * written ) {
  int m, result = 0;
  void * buffer = data;
  size_t w;
  MIDIRunningStatus status = 0;
  MIDITimestamp     timestamp2;
  MIDIVarLen        time_diff;
  
  pr_debug("RTPMIDI encoding message\n");

  for( m=0; (size>0) && (messages!=NULL) && (messages->message!=NULL); m++ ) {
	  pr_debug("RTPMIDI encode message nr: %i\n",m);
    MIDIMessageGetTimestamp( messages->message, &timestamp2 );
	pr_debug("RTPMIDI got timestamp\n");
    time_diff = ( timestamp2 > timestamp ) ? ( timestamp2 - timestamp ) : 0;
    timestamp = timestamp2;
    if( m == 0 ) {
      info->zero = time_diff ? 1 : 0;
    }
    if( m > 0 || info->zero == 1 ) {
		pr_debug("RTPMIDI vor write varlen\n");
      MIDIUtilWriteVarLen( &time_diff, size, buffer, &w );
	  pr_debug("RTPMIDI written varlen\n");
      _advance_buffer( &size, &buffer, w );
	  pr_debug("RTPMIDI advanced\n");
    }
	pr_debug("RTPMIDI will get running status\n");
    MIDIMessageEncodeRunningStatus( messages->message, &status, size, buffer, &w );
	pr_debug("RTPMIDI will advance buffer\n");
    _advance_buffer( &size, &buffer, w );
	pr_debug("RTPMIDI advanced\n");
    messages = messages->next;
  }

  info->len = buffer - data;
  
  *written = buffer - data;
  return result;
}


/**
 * @brief Send MIDI messages over an RTPSession.
 * Broadcast the messages to all connected peers. Store the number of sent messages
 * in @c count, if the @c info argument was specified it will be populated with the
 * packet info of the last sent packet.
 * The peer's control structures will be updated with the required journalling
 * information.
 * @public @memberof RTPMIDISession
 * @param session  The session.
 * @param messages A pointer to a list of @c size message pointers.
 * @retval 0 On success.
 * @retval >0 If the message could not be sent.
 */
//int RTPMIDISessionSend( struct RTPMIDISession * session, struct MIDIMessageList * messages ) {
int RTPMIDISessionSend( struct RTPMIDISession * session, struct MIDIMessageList * messages ) {
  int result = 0;
  struct iovec iov[3];
  size_t written = 0;
  size_t size    = session->size;
  void * buffer  = session->buffer;

  struct RTPPeer        * peer    = NULL;
  //struct RTPMIDIJournal * journal = NULL;
  struct RTPMIDIInfo    * minfo   = &(session->midi_info);
  struct RTPPacketInfo  * info    = &(session->rtp_info);

  MIDITimestamp timestamp;

  MIDIMessageGetTimestamp( messages->message, &timestamp );
  
  pr_debug("RTPMIDI timestamped message\n");

  info->peer            = 0;
  info->padding         = 0;
  info->extension       = 0;
  info->csrc_count      = 0;
  info->marker          = 0;
  info->payload_type    = 97;
  info->sequence_number = 0; /* filled out by rtp */
  info->timestamp       = timestamp; /* filled out by rtp but shouldn't */

  minfo->journal = 0;
  minfo->phantom = 0;
  minfo->zero    = 0;

  _rtpmidi_encode_messages( minfo, timestamp, messages, size, buffer, &written );
  iov[1].iov_base = buffer;
  iov[1].iov_len  = written;
  _advance_buffer( &size, &buffer, written );

  _rtpmidi_encode_header( minfo, size, buffer, &written );
  iov[0].iov_base = buffer;
  iov[0].iov_len  = written;
  _advance_buffer( &size, &buffer, written );

  /* send encoded messages to each peer
   * each peer has its own journal */
  result = RTPSessionNextPeer( session->rtp_session, &peer );
  while( peer != NULL ) {
    //if( minfo->journal ) {
    //  journal = NULL; /* peer out journal */
    //  _rtpmidi_journal_encode( session, journal, size, buffer, &written );
    //  iov[2].iov_base = buffer;
    //  iov[2].iov_len  = written;
    //  _advance_buffer( &size, &buffer, written );
    //} else {
      iov[2].iov_base = NULL;
      iov[2].iov_len  = 0;
    //}

    info->peer   = peer;
    info->iovlen = ( minfo->journal ) ? 3 : 2;
    info->iov    = &(iov[0]);
    info->payload_size = iov[0].iov_len + iov[1].iov_len + iov[2].iov_len;

    result = RTPSessionSendPacket( session->rtp_session, info );

    if( result == 0 && minfo->journal ) {
      //_rtpmidi_journal_encode_messages( journal, info->sequence_number, messages );
    }

    RTPSessionNextPeer( session->rtp_session, &peer );
  }

  return result;
}

