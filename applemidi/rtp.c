#include <net/sock.h>
#include <linux/slab.h>

#include "rtp.h"

#define RTP_MAX_PEERS 16
#define RTP_BUF_LEN   512
#define RTP_IOV_LEN   16

struct RTPAddress {
	unsigned long ssrc;
	int size;
	struct sockaddr_in addr;
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
struct RTPPeer * RTPPeerCreate( unsigned long ssrc, int size, struct sockaddr_in * addr ) {
  struct RTPPeer * peer = kmalloc( sizeof( struct RTPPeer ), GFP_KERNEL );
  peer->refs = 1;
  peer->address.ssrc = ssrc;
  peer->address.size = size;
  memcpy( &(peer->address.addr), addr, sizeof(struct sockaddr_in) );
  pr_info("added RTP client %pI4:%d\n",&peer->address.addr.sin_addr.s_addr,peer->address.addr.sin_port);
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
 * @brief Obtain the size of the address and a pointer to it's content.
 * @public @memberof RTPPeer
 * @param peer The peer.
 * @param size The size.
 * @param addr The address.
 * @retval 0 on success.
 * @retval >0 if the address could not be obtained.
 */
int RTPPeerGetAddress( struct RTPPeer * peer, int * size, struct sockaddr_in ** addr ) {
  if( size == NULL || addr == NULL ) return 1;
  *size =   peer->address.size;
  *addr =  &(peer->address.addr);
  return 0;
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
 * @brief Advance the pointer to the next peer.
 * Given a @c NULL pointer the first peer will be returned. When the
 * last peer was reached a @c NULL pointer will be returned.
 * @public @memberof RTPSession
 * @param session The session.
 * @param peer The peer.
 * @retval 0 on success.
 * @retval >0 if the given peer does not exist.
 */
int RTPSessionNextPeer( struct RTPSession * session, struct RTPPeer ** peer ) {
  int i;
  if( peer == NULL ) return 1;
  if( *peer == NULL ) {
    i=-1;
  } else {
    for( i=0; i < RTP_MAX_PEERS; i++ ) {
      if( session->peers[i] == *peer ) {
        break;
      }
    }
    if( i == RTP_MAX_PEERS ) return 1;
  }

  do {
     i++;
  } while( i < RTP_MAX_PEERS && session->peers[i] == NULL );
  if( i == RTP_MAX_PEERS ) {
    *peer = NULL;
  } else {
    *peer = session->peers[i];
  }
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

static int _rtp_encode_header( struct RTPPacketInfo * info, size_t size, void * data, size_t * written ) {
  int i, j;
  unsigned char * buffer = data;
  size_t header_size = 12 + ( info->csrc_count * 4 );
  
  pr_debug("RTP encode header\n");

  if( size < header_size ) return 1;
    
  buffer[0] = 0x80
            | ( info->padding ? 0x20 : 0 )
            | ( info->extension ? 0x10 : 0 )
            | ( info->csrc_count & 0x0f );
  
  buffer[1] = ( info->payload_type & 0x7f )
            | ( info->marker ? 0x80 : 0x00 );
  
  buffer[2] = ( info->sequence_number >> 8 ) & 0xff;
  buffer[3] =   info->sequence_number        & 0xff;

  buffer[4] = ( info->timestamp >> 24 ) & 0xff;
  buffer[5] = ( info->timestamp >> 16 ) & 0xff;
  buffer[6] = ( info->timestamp >> 8 )  & 0xff;
  buffer[7] =   info->timestamp         & 0xff;

  buffer[8]  = ( info->ssrc >> 24 ) & 0xff;
  buffer[9]  = ( info->ssrc >> 16 ) & 0xff;
  buffer[10] = ( info->ssrc >> 8 )  & 0xff;
  buffer[11] =   info->ssrc         & 0xff;

  for( i=0, j=0; i<info->csrc_count; i++, j+=4 ) {
    buffer[12+j] = ( info->csrc[i] >> 24 ) & 0xff;
    buffer[13+j] = ( info->csrc[i] >> 16 ) & 0xff;
    buffer[14+j] = ( info->csrc[i] >> 8 )  & 0xff;
    buffer[15+j] =   info->csrc[i]         & 0xff;
  }
  
  *written = header_size;
  return 0;
}

static int _rtp_encode_extension( struct RTPPacketInfo * info, size_t size, void * data, size_t * written ) {
  int i;
  unsigned char * buffer = data;
  size_t ext_header_size;
  
  pr_debug("RTP encode extension\n");
  
  if( info->extension ) {
    if( info->iovlen < 1 ) return 1;
    ext_header_size = info->iov[0].iov_len;
    if( ext_header_size % 4 || ext_header_size == 0 ) {
      /* fill up to whole 4 bytes words */
      ext_header_size += 4 - (info->iov[0].iov_len % 4);
    }

    memcpy( buffer, info->iov[0].iov_base, info->iov[0].iov_len );
    i = ext_header_size / 4;
    buffer[2] = ( i >> 8 ) & 0xff;
    buffer[3] =   i        & 0xff;
  } else {
    ext_header_size = 0;
  }
  
  *written = ext_header_size;
  return 0;
}

static int _rtp_encode_padding( struct RTPPacketInfo * info, size_t size, void * data, size_t * written ) {
  unsigned char * buffer = data;
  
  pr_debug("RTP encode padding\n");
  
  if( info->padding ) {
    if( size < info->padding ) return 1;
    buffer[info->padding-1] = info->padding;
  }
  *written = info->padding;
  return 0;
}

static void _advance_buffer( size_t * size, void ** buffer, size_t bytes ) {
	pr_debug("RTP advance buf\n");
  *size   -= bytes;
  *buffer += bytes;
}

static void _append_iov( size_t * iovlen, struct iovec * iov, size_t size, void * buffer ) {
	pr_debug("RTP append iov\n");
  iov[*iovlen].iov_len  = size;
  iov[*iovlen].iov_base = buffer;
  *iovlen += 1;
}


/**
 * @brief Send an RTP packet.
 * @public @memberof RTPSession
 * @param session The session.
 * @param info The packet info.
 * @retval 0 On success.
 * @retval >0 If the message could not be sent.
 */
int RTPSessionSendPacket( struct RTPSession * session, struct RTPPacketInfo * info ) {
  size_t size, written = 0, iovlen = 0;
  void * buffer;
  struct msghdr msg;
  struct iovec  iov[RTP_IOV_LEN+3];
  ssize_t bytes_sent;
  
  struct sockaddr_in *a;
  
  mm_segment_t oldfs;
  
  pr_debug("RTP send message\n");
  
  if( info == NULL || info->peer == NULL ) return 1;
  if( info->iovlen > RTP_IOV_LEN ) return 1;
  
  size   = session->buflen;
  buffer = session->buffer;

  info->ssrc            = session->self.ssrc;
  info->sequence_number = info->peer->out_seqnum + 1;

  info->total_size = 0;
  _rtp_encode_header( info, size, buffer, &written );
  _append_iov( &iovlen, &(iov[0]), written, buffer );
  _advance_buffer( &size, &buffer, written );
  info->total_size += written;
  if( info->extension ) {
    _rtp_encode_extension( info, size, &buffer, &written );
    _append_iov( &iovlen, &(iov[0]), written, buffer );
    _advance_buffer( &size, &buffer, written );
    info->total_size += written;
  }
  info->payload_size = 0;
  while( (iovlen-1)<info->iovlen ) {
    info->payload_size += info->iov[iovlen-1].iov_len;
    _append_iov( &iovlen, &(iov[0]), info->iov[iovlen-1].iov_len, info->iov[iovlen-1].iov_base );
  }
  info->total_size += info->payload_size;
  if( info->padding ) {
    _rtp_encode_padding( info, size, &buffer, &written );
    _append_iov( &iovlen, &(iov[0]), written, buffer );
    _advance_buffer( &size, &buffer, written );
    info->total_size += written;
  }

  pr_debug("RTP Sending RTP message consisting of %i iovecs.\n", (int) iovlen );
  int i, j;
  for( i=0; i<iovlen; i++ ) {
    pr_debug("[%i] iov_len: %i, iov_base: %p\n", i, (int) iov[i].iov_len, iov[i].iov_base );
    for( j=0; j<iov[i].iov_len; j++ ) {
      unsigned char c = *((unsigned char*)iov[i].iov_base+j);
      if( (j+1) % 8 == 0 || j+1 == iov[i].iov_len ) {
        pr_debug("0x%02x\n", c );
      } else {
        pr_debug("0x%02x ", c );
      }
    }
  }
  
  
	//to.sin_family = AF_INET;
	//to.sin_addr.s_addr = a->sin_addr.s_addr;
	//to.sin_port=a->sin_port;
  a = &(info->peer->address.addr);
  pr_debug("send %i bytes to %pI4:%i on s(%p)\n", info->total_size, &a->sin_addr.s_addr, ntohs(a->sin_port ), session->socket->sk );

  msg.msg_name       =  &(info->peer->address.addr);
  msg.msg_namelen    = info->peer->address.size;
  //msg.msg_namelen    = info->peer->address.size;
  msg.msg_iov        = &(iov[0]);
  msg.msg_iovlen     = iovlen;
  msg.msg_control    = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags      = 0;


	oldfs = get_fs();
	set_fs(KERNEL_DS);
	bytes_sent=sock_sendmsg(session->socket, &msg, info->total_size);
	set_fs(oldfs);
  //bytes_sent = sendmsg( session->socket, &msg, 0 );
	
	pr_debug("RTP bytes sent: %i of %i\n",bytes_sent,info->total_size);

  if( bytes_sent != info->total_size ) {
    return bytes_sent;
  } else if( msg.msg_flags != 0 ) {
    return 1;
  } else {
    info->peer->out_seqnum    = info->sequence_number;
    info->peer->out_timestamp = info->timestamp;
    return 0;
  }
}

