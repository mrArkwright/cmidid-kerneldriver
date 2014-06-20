#include <net/sock.h>
#include <linux/slab.h>

#include "rtp.h"

#define RTP_MAX_PEERS 16
#define RTP_BUF_LEN   512
#define RTP_IOV_LEN   16

struct RTPAddress {
	unsigned long ssrc;
	int size;
	struct sockaddr_storage addr;
};

struct RTPPeer {
	size_t refs;
	struct RTPAddress address;
	unsigned long in_timestamp;
	unsigned long out_timestamp;
	unsigned long in_seqnum;
	unsigned long out_seqnum;
	void *info;
};

struct RTPSession {
	size_t refs;

	struct socket *socket;

	struct RTPAddress self;
	struct RTPPeer *peers[RTP_MAX_PEERS];
	struct RTPPacketInfo info;

	struct iovec iov[RTP_IOV_LEN];
	size_t buflen;
	void *buffer;
};

static void _init_addr_with_socket(struct RTPAddress *address,
				   struct socket *sock)
{
	address->ssrc = 0;
	address->size = sizeof(address->addr);
	//getsockname( socket, (struct sockaddr *) &(address->addr), &(address->size) );
}

static void _session_randomize_ssrc(struct RTPSession *session)
{
	/*static struct timeval tv = { 0, 0 };
	   unsigned long seed;
	   if( tv.tv_sec == 0 ) {
	   gettimeofday( &tv, NULL );
	   seed = tv.tv_sec * 1000000 + tv.tv_usec;
	   srandom( seed );
	   } */
	get_random_bytes(&session->self.ssrc, sizeof(session->self.ssrc));
	//session->self.ssrc = random();
}

/**
 * @brief Create an RTPPeer instance.
 * Allocate space and initialize an RTPPeer instance.
 * @public @memberof RTPPeer
 * @param ssrc The synchronization source identifier that uniquely identifies the peer.
 * @param size The size of the address pointed to by @c addr.
 * @param addr A pointer to an address that can be used to send packets to the client.
 * @return a pointer to the created peer structure on success.
 * @return a @c NULL pointer if the peer could not created.
 */
struct RTPPeer * RTPPeerCreate( unsigned long ssrc, int size, struct sockaddr * addr ) {
  struct RTPPeer * peer = kmalloc( sizeof( struct RTPPeer ), GFP_KERNEL );
  peer->refs = 1;
  peer->address.ssrc = ssrc;
  peer->address.size = size;
  memcpy( &(peer->address.addr), addr, size );
  peer->in_seqnum      = 0;
  peer->in_timestamp   = 0;
  peer->out_seqnum     = 0;
  peer->out_timestamp  = 0;
  peer->info = NULL;
  return peer;
}

/**
 * @brief Retain an RTPPeer instance.
 * Increment the reference counter of a peer so that it won't be destroyed.
 * @public @memberof RTPPeer
 * @param peer The peer.
 */
void RTPPeerRetain( struct RTPPeer * peer ) {
  peer->refs++;
}

/**
 * @brief Destroy an RTPPeer instance.
 * Free all resources occupied by the peer.
 * @public @memberof RTPPeer
 * @param peer The peer.
 */
void RTPPeerDestroy(struct RTPPeer *peer)
{
	kfree(peer);
}

/**
 * @brief Release an RTPPeer instance.
 * Decrement the reference counter of a peer. If the reference count
 * reached zero, destroy the peer.
 * @public @memberof RTPPeer
 * @param peer The peer.
 */
void RTPPeerRelease(struct RTPPeer *peer)
{
	if (!--peer->refs) {
		RTPPeerDestroy(peer);
	}
}

/**
 * @brief Create an RTPSession instance.
 * Allocate space and initialize an RTPSession instance.
 * @public @memberof RTPSession
 * @param socket The socket to use for communication.
 * @return a pointer to the created controller structure on success.
 * @return a @c NULL pointer if the controller could not created.
 */
struct RTPSession *RTPSessionCreate(struct socket *sock)
{
	struct RTPSession *session =
	    kmalloc(sizeof(struct RTPSession), GFP_KERNEL);
	int i;
	session->refs = 1;
	session->socket = sock;

	_init_addr_with_socket(&(session->self), sock);
	for (i = 0; i < RTP_MAX_PEERS; i++) {
		session->peers[i] = NULL;
	}

	session->buflen = RTP_BUF_LEN;
	session->buffer = kmalloc(session->buflen, GFP_KERNEL);
	if (session->buffer == NULL) {
		session->buflen = 0;
	}
	session->iov[0].iov_base = session->buffer;
	session->iov[9].iov_len = session->buflen;
	for (i = 1; i < RTP_IOV_LEN; i++) {
		session->iov[i].iov_base = NULL;
		session->iov[i].iov_len = 0;
	}

	_session_randomize_ssrc(session);

	session->info.peer = NULL;
	session->info.padding = 0;
	session->info.extension = 0;
	session->info.csrc_count = 0;
	session->info.marker = 0;
	session->info.payload_type = 0;
	session->info.ssrc = session->self.ssrc;
	session->info.iovlen = RTP_IOV_LEN;
	session->info.iov = &(session->iov[0]);

	return session;
}

/**
 * @brief Destroy an RTPSession instance.
 * Free all resources occupied by the session.
 * @public @memberof RTPSession
 * @param session The session.
 */
void RTPSessionDestroy(struct RTPSession *session)
{
	int i;
	for (i = 0; i < RTP_MAX_PEERS; i++) {
		if (session->peers[i] != NULL) {
			RTPPeerRelease(session->peers[i]);
		}
	}
	kfree(session->buffer);
	kfree(session);
}

/**
 * @brief Retain an RTPSession instance.
 * Increment the reference counter of a session so that it won't be destroyed.
 * @public @memberof RTPSession
 * @param session The session.
 */
void RTPSessionRetain(struct RTPSession *session)
{
	session->refs++;
}

void RTPSessionRelease(struct RTPSession *session)
{
	if (!--session->refs) {
		RTPSessionDestroy(session);
	}
}


/**
 * @brief Add an RTPPeer to the session.
 * Lookup the peer using the (pseudo) hash-table, add it to the list and retain it.
 * The peer will be included when data is sent via RTPSessionSendPayload.
 * @public @memberof RTPSession
 * @param session The session.
 * @param peer The peer to add.
 * @retval 0 on success.
 * @retval >0 if the peer could not be added.
 */
int RTPSessionAddPeer( struct RTPSession * session, struct RTPPeer * peer ) {
  int i, off, p;
  off = peer->address.ssrc % RTP_MAX_PEERS;
  for( i=0; i < RTP_MAX_PEERS; i++ ) {
    p = (i+off)%RTP_MAX_PEERS;
    if( session->peers[p] == NULL ) {
      session->peers[p] = peer;
      RTPPeerRetain( peer );
      return 0;
    }
  }
  return 1;
}


/**
 * @brief Remove an RTPPeer from the session.
 * Lookup the peer using the (pseudo) hash-table, remove it from the list and release it.
 * @public @memberof RTPSession
 * @param session The session.
 * @param peer The peer to remove.
 * @retval 0 on success.
 * @retval >0 if the peer could not be removed.
 */
int RTPSessionRemovePeer( struct RTPSession * session, struct RTPPeer * peer ) {
  int i, off, p;
  off = peer->address.ssrc % RTP_MAX_PEERS;
  for( i=0; i < RTP_MAX_PEERS; i++ ) {
    p = (i+off)%RTP_MAX_PEERS;
    if( session->peers[p] == peer ) {
      session->peers[p] = NULL;
      RTPPeerRelease( peer );
      return 0;
    }
  }
  return 1;
}


int RTPSessionGetSSRC( struct RTPSession * session, unsigned long * ssrc ) {
  if( ssrc == NULL ) return 1;
  *ssrc = session->self.ssrc;
  return 0;
}


/**
 * Retrieve peer information by looking up the given SSRC identifier.
 * @public @memberof RTPSession
 * @param session The session.
 * @param peer The peer.
 * @param ssrc The SSRC.
 * @retval 0 on success.
 * @retval >0 if no peer with the given ssrc was found.
 */
int RTPSessionFindPeerBySSRC( struct RTPSession * session, struct RTPPeer ** peer,
                              unsigned long ssrc ) {
  int i, off, p;
  off = ssrc % RTP_MAX_PEERS;
  for( i=0; i < RTP_MAX_PEERS; i++ ) {
    p = (i+off)%RTP_MAX_PEERS;
    if( session->peers[p] != NULL ) {
      if( session->peers[p]->address.ssrc == ssrc ) {
        *peer = session->peers[p];
        return 0;
      }
    }
  }
  return 1;
}

