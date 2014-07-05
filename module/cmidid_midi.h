#ifndef CMIDID_MIDI_H
#define CMIDID_MIDI_H

int midi_init(void);

void send_note(unsigned char notevalue, unsigned char velocity);

void midi_exit(void);

#endif
