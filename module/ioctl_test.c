#include <stdint.h>
#include <stdio.h>

#include "cmidid_ioctl.h"
#include <stdio.h>

void print_options()
{
	printf("You can set different options for the cmidid device:\n"
	       "[1] Calibrate last hit as minimal key velocity\n"
	       "[2] Calibrate last hit as maximal key velocity\n"
	       "[3] Set velocity curve to linear\n"
	       "[4] Set velocity curve to concave\n"
	       "[5] Set velocity curve to convex\n"
	       "[6] Set velocity curve to saturated\n"
	       "[7] Transpose\nOption:");
}

int main(int argc, char *argv[])
{
	char *file_name = "/dev/cmidid";
	int fd;
	int value;
	int err = 0;
	uint32_t min_time;

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
			min_time = ioctl(fd, CMIDID_CALIBRATE_MIN_TIME);
			printf("min time set to %d\n", min_time);
			break;
		case 2:
			min_time = ioctl(fd, CMIDID_CALIBRATE_MAX_TIME);
			printf("max time set to %d\n", min_time);
			break;
		case 3:
			min_time = ioctl(fd, CMIDID_VEL_CURVE_LINEAR);
			printf("Velocity curve set linear !\n");
			break;
		case 4:
			min_time = ioctl(fd, CMIDID_VEL_CURVE_CONCAVE);
			printf("Velocity curve set to concave!\n");
			break;
		case 5:
			min_time = ioctl(fd, CMIDID_VEL_CURVE_CONVEX);
			printf("Velocity curve set to convex\n");
			break;
		case 6:
			min_time = ioctl(fd, CMIDID_VEL_CURVE_SATURATED);
			printf("Velocity curve set to saturated!\n");
			break;
		case 7:
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
