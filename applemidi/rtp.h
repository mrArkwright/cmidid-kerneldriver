#ifndef RTP_H
#define RTP_H

#include <net/sock.h>
#include <linux/slab.h>

struct RTPPacketInfo {
  struct RTPPeer * peer;
  unsigned char  padding;
  unsigned char  extension;
  unsigned char  csrc_count;
  unsigned char  marker;
  unsigned char  payload_type;
  unsigned short sequence_number;
  unsigned long  timestamp;
  unsigned long  ssrc;
  unsigned long  csrc[16];
  size_t total_size;
  size_t payload_size;
  size_t iovlen;
  struct iovec * iov;
};

struct RTPPeer * RTPPeerCreate( unsigned long ssrc, int size, struct sockaddr * addr );
void RTPPeerRetain( struct RTPPeer * peer );
void RTPPeerRelease( struct RTPPeer * peer );
int RTPPeerGetAddress( struct RTPPeer * peer, int * size, struct sockaddr ** addr );

struct RTPSession * RTPSessionCreate( struct socket *sock );
void RTPSessionDestroy( struct RTPSession * session );
void RTPSessionRetain( struct RTPSession * session );
void RTPSessionRelease( struct RTPSession * session );

int RTPSessionAddPeer( struct RTPSession * session, struct RTPPeer * peer );
int RTPSessionRemovePeer( struct RTPSession * session, struct RTPPeer * peer );

int RTPSessionGetSSRC( struct RTPSession * session, unsigned long * ssrc );
int RTPSessionNextPeer( struct RTPSession * session, struct RTPPeer ** peer );

int RTPSessionFindPeerBySSRC( struct RTPSession * session, struct RTPPeer ** peer,
                              unsigned long ssrc );
							  
#endif