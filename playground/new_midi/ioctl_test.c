//#include <stdio.h>
//#include <sys/types.h>
#include <fcntl.h>
//#include <unistd.h>
//#include <string.h>
//#include <sys/ioctl.h>

int main(int argc, char *argv[]) {
	char *file_name = "/dev/new_midi";
	int fd;
	
	fd = open(file_name, O_RDWR);
	if (fd == -1) {
		perror("new_midi open");
		return 2;
	}
	
	ioctl(fd, 0);
	
	close (fd);
	
	return 0;
}