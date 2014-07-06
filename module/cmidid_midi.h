#ifndef CMIDID_MIDI_H
#define CMIDID_MIDI_H

int midi_init(void);

void note_on(unsigned char note, unsigned char velocity);

void note_off(unsigned char note);

void midi_exit(void);

#endif
