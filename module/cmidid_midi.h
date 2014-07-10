#ifndef CMIDID_MIDI_H
#define CMIDID_MIDI_H

signed char cmidid_transpose(signed char transpose);

void cmidid_note_on(unsigned char note, unsigned char velocity);
void cmidid_note_off(unsigned char note);

int cmidid_midi_init(void);
void cmidid_midi_exit(void);

#endif
