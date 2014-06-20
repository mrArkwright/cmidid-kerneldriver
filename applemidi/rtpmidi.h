#ifndef RTPMIDI_H
#define RTPMIDI_H

struct RTPMIDISession * RTPMIDISessionCreate( struct RTPSession * session );
void RTPMIDISessionRelease( struct RTPMIDISession * session );

#endif