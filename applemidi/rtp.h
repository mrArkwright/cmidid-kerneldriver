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

struct RTPSession * RTPSessionCreate( struct socket *sock );
void RTPSessionDestroy( struct RTPSession * session );
void RTPSessionRetain( struct RTPSession * session );
void RTPSessionRelease( struct RTPSession * session );