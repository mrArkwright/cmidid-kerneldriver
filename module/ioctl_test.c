#include "cmidid_ioctl.h"

int main(int argc, char *argv[])
{
	char *file_name = "/dev/cmidid";
	int fd;

	fd = open(file_name, 0);
	if (fd == -1) {
		perror("open /dev/cmidid failed\n");
		return 2;
	}

	ioctl(fd, CMIDID_CALIBRATE);

	close(fd);

	return 0;
}
