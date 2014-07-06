#ifndef APPLEMIDI_H
#define APPLEMIDI_H

#include <net/sock.h>
#include <linux/spinlock.h>

#include "midi.h"

#define APPLEMIDI_PROTOCOL_SIGNATURE          0xffff

#define APPLEMIDI_COMMAND_INVITATION          0x494e /** "IN" on control & rtp port */
#define APPLEMIDI_COMMAND_INVITATION_REJECTED 0x4e4f /** "NO" on control & rtp port */
#define APPLEMIDI_COMMAND_INVITATION_ACCEPTED 0x4f4b /** "OK" on control & rtp port */
#define APPLEMIDI_COMMAND_ENDSESSION          0x4259 /** "BY" on control port */
#define APPLEMIDI_COMMAND_SYNCHRONIZATION     0x434b /** "CK" on rtp port */
#define APPLEMIDI_COMMAND_RECEIVER_FEEDBACK   0x5253 /** "RS" on control port */


struct AppleMIDICommand {
	struct RTPPeer *peer;	/* use peers sockaddr instead .. we get
				 * initialization problems otherwise */
	struct sockaddr_in addr;
	int size;
	/* unsigned short channel; */
	unsigned short type;
	union {
		struct {
			unsigned long version;
			unsigned long token;
			unsigned long ssrc;
			char name[64];
		} session;
		struct {
			unsigned long ssrc;
			unsigned long count;
			unsigned long long timestamp1;
			unsigned long long timestamp2;
			unsigned long long timestamp3;
		} sync;
		struct {
			unsigned long ssrc;
			unsigned long seqnum;
		} feedback;
	} data;
};


struct MIDIDriverAppleMIDI {
    	spinlock_t lock;
	struct MIDIDriver base;
	struct socket *control_socket;
	struct socket *rtp_socket;
	unsigned short port;
	unsigned char accept;
	unsigned char sync;
	unsigned long token;
	char name[32];
	
	struct timer_list timer;

	struct AppleMIDICommand command;

	struct RTPPeer *peer;
	struct RTPSession *rtp_session;
	struct RTPMIDISession *rtpmidi_session;

	struct MIDIMessageQueue *in_queue;
	struct MIDIMessageQueue *out_queue;
};

#endif
