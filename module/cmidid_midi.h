#ifndef CMIDID_MIDI_H
#define CMIDID_MIDI_H

int midi_init(void);

void midi_exit(void);

void send_note(unsigned char, unsigned char);

#endif
