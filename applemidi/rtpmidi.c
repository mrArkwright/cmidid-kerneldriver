
#include "rtp.h"

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
