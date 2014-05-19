#include <alsa/asoundlib.h>
#include <unistd.h>

#define msleep(ms) usleep(1000*ms)

int main() {
	int status;
	int mode = SND_RAWMIDI_SYNC;
	snd_rawmidi_t* midiout = NULL;
	const char* portname = "virtual";
	
	if ((status = snd_rawmidi_open(NULL, &midiout, portname, mode)) < 0) {
		printf("error 0, status %d\n", status);
		return 1;
	}
	
	char noteon[3] = {0x99, 60, 100};
	char noteoff[3] = {0x89, 60, 100};
	char inc = 0;
	
	
	while(1) {
		noteon[1] = inc;
		noteoff[1] = inc;
		snd_rawmidi_write(midiout, noteon, 3);
		printf("note on\n");
		
		msleep(100);
		
		snd_rawmidi_write(midiout, noteoff, 3);
		printf("note off\n");
		
		msleep(25);

		inc++;
		if (inc >= 100) inc = 0;
	}
	
	return 0;
}
