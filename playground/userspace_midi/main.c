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
	char inc = 60;
	
	int key;
	int map[250];
	
	for(int i=0;i<250;i++)
	{
		map[i]=-1;
	}	
        map['l']=74;
        map['k']=72;
        map['j']=71;
	map['h']=69;
	map['g']=67;
        map['f']=65;
        map['d']=64;
        map['s']=62;
        map['a']=60;

	//system("stty raw"); 
	
	while(1) {
		key = getchar();
		
		if(map[key]!=-1)
		{ inc=map[key];}
		else
		{
			continue;
		}
		if(key=='q')
		{
			break;
		}		

		noteon[1] = inc;
		noteoff[1] = inc;
		snd_rawmidi_write(midiout, noteon, 3);
		noteon[1] = inc+4;
		snd_rawmidi_write(midiout, noteon, 3);
                noteon[1] = inc+7;
		snd_rawmidi_write(midiout, noteon, 3);
		noteon[1] = inc-13;
                snd_rawmidi_write(midiout, noteon, 3);
		noteon[1] = inc+13;
                snd_rawmidi_write(midiout, noteon, 3);
		//	printf("note on\n");
		
		msleep(500);
		
		snd_rawmidi_write(midiout, noteoff, 3);
		noteoff[1] = inc+4;
                snd_rawmidi_write(midiout, noteoff, 3);
                noteoff[1] = inc+7;
                snd_rawmidi_write(midiout, noteoff, 3);
		noteoff[1] = inc-13;
                snd_rawmidi_write(midiout, noteoff, 3);
                noteoff[1] = inc+13;
                snd_rawmidi_write(midiout, noteoff, 3);
		//printf("note off\n");
		
		//msleep(25);

		//getchar();

		//inc+=7;
		//if (inc >= 68) inc = 60;
	}
	
	//system("stty cooked"); 
	
	return 0;
}
