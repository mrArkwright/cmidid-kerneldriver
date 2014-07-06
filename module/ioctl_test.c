#include "cmidid_ioctl.h"
#include <stdio.h>

void print_options()
{
	printf("You can set different options for the cmidid device:\n"
	       "[1] Calibrate\n" "[2] Transpose\n");
}

int main(int argc, char *argv[])
{
	char *file_name = "/dev/cmidid";
	int fd;
	int value;
	int err = 0;

	fd = open(file_name, 0);
	if (fd == -1) {
		perror("open /dev/cmidid failed\n");
		return 2;
	}

	err = 1;
	while (err == 1) {
		err = scanf("Option:%d\n", &value);
		switch (value) {
		case 1:
			printf("Calibrated to:%ld\n",
			       ioctl(fd, CMIDID_CALIBRATE));
			break;
		case 2:
			printf("Current transpose is %d add:",
			       ioctl(fd, CMIDID_TRANSPOSE, 0));
			err = scanf("%d\n", &value);
			ioctl(fd, CMIDID_TRANSPOSE, value);
			break;
		}
	}

	close(fd);
	return 0;
}
