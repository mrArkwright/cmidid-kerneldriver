#ifndef CMIDID_MAIN_H
#define CMIDID_MAIN_H

#include <linux/kernel.h>
#include <linux/device.h>


#define MODULE_NAME "cmidid"

/* Maximum size for input array requested_gpios. */
#define MAX_REQUEST 32

/***********************************************************************
 * ioctl Settings
 **********************************************************************/

#define IOCTL_DEV_NAME MODULE_NAME

/***********************************************************************
 * Module parameter settings
 **********************************************************************/

#define PARM_VIS_FLAGS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

#endif
