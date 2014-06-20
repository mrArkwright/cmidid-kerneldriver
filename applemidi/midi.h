
#ifndef MIDI_H
#define MIDI_H

#include "clock.h"

struct MIDIDriver {
  size_t refs;
  //struct MIDIRunloopSource * rls;
  //struct MIDIPort * port;
  struct MIDIClock * clock;
  //int (*send)( void * driver, struct MIDIMessage * message );
  //void (*destroy)( void * driver );
};

void MIDIDriverInit( struct MIDIDriver * driver, char * name, MIDISamplingRate rate );

void MIDIDriverRelease( struct MIDIDriver * driver );

#endif