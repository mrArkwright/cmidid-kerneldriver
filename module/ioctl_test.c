#include <stdint.h>
#include <stdio.h>

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

	uint32_t min_time = ioctl(fd, CMIDID_CALIBRATE_MIN_TIME);

	printf("min time set to %d\n", min_time);

	close(fd);

	return 0;
}
