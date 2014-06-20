
#ifndef CLOCK_H
#define CLOCK_H

typedef unsigned int   MIDISamplingRate;
typedef long long      MIDITimestamp;

#define MIDI_SAMPLING_RATE_DEFAULT      0

struct MIDIClock {
/**
 * @privatesection
 * @cond INTERNALS
 */
  int              refs;
  MIDITimestamp    offset;
  MIDISamplingRate rate;
  unsigned long long numer;
  unsigned long long denom;
/** @endcond */
};

struct MIDIClock * MIDIClockProvide( MIDISamplingRate rate );
void MIDIClockRetain( struct MIDIClock * clock );
void MIDIClockRelease( struct MIDIClock * clock );

int MIDIClockGetNow( struct MIDIClock * clock, MIDITimestamp * now );

#endif