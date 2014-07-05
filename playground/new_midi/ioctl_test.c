#include "new_midi.h"

int main(int argc, char *argv[]) {
	char *file_name = "/dev/new_midi";
	int fd;
	
	fd = open(file_name, 0);
	if (fd == -1) {
		perror("new_midi open");
		return 2;
	}
	
	ioctl(fd, NEWMIDI_BLUPP);
	
	close (fd);
	
	return 0;
}