#ifndef RTPMIDI_H
#define RTPMIDI_H

#include "message.h"
struct RTPSession;

struct RTPMIDISession *RTPMIDISessionCreate(struct RTPSession *session);
void RTPMIDISessionRelease(struct RTPMIDISession *session);

int RTPMIDISessionSend(struct RTPMIDISession *session,
		       struct MIDIMessageList *messages);

#endif