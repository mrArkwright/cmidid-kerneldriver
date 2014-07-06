#include "cmidid_ioctl.h"
#include <stdio.h>

void print_options()
{
	printf("You can set different options for the cmidid device:\n"
	       "[1] Calibrate\n" "[2] Transpose\nOption:");
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
		print_options();
		err = scanf("%d", &value);
		switch (value) {
		case 1:
			printf("Calibrated to:%ld\n",
			       ioctl(fd, CMIDID_CALIBRATE));
			break;
		case 2:
			printf("Current transpose is %ld add:",
			       ioctl(fd, CMIDID_TRANSPOSE, 0) - 128);
			err = scanf("%d", &value);
			printf("\nTranspose set to:%ld\n",
			       ioctl(fd, CMIDID_TRANSPOSE, value) - 128);
			break;
		default:
			printf("Unknown option");
			break;
		}
	}

	close(fd);
	return 0;
}
