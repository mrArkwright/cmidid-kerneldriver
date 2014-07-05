#ifndef CMIDID_MAIN_H
#define CMIDID_MAIN_H

#include <linux/kernel.h>
#include <linux/device.h>


#define MODULE_NAME "cmidid"

/* Maximum size for input array requested_gpios. */
#define MAX_REQUEST 32

/***********************************************************************
 * ioctl Settings
 *
 * The device number (major) should be between 0 and 255.
 * I guess a large number is more likely to avoid clashes.
 **********************************************************************/

#define IOCTL_DEV_NAME MODULE_NAME
#define IOCTL_DEV_NUMBER 0xCC /* 204 */

/***********************************************************************
 * Module parameter settings
 **********************************************************************/

#define PARM_VIS_FLAGS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)


/**********************************************************************
 * Logging wrapper functions
 *
 * The `err' macro is really useful, because it prints the filename,
 * line number and function name alongside the error message.
 **********************************************************************/
extern struct device *cmidid_device;

#define info(fmt, ...) \
        dev_info(cmidid_device, "[%s] " pr_fmt(fmt), \
                        __func__, ##__VA_ARGS__)

#define dbg(fmt, ...) \
        dev_dbg(cmidid_device, "[%s:%d %s] Debug: " pr_fmt(fmt), \
                        __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define warn(fmt, ...) \
        dev_warn(cmidid_device, "[%s] " pr_fmt(fmt), \
                        __func__, ##__VA_ARGS__)

#define err(fmt, ...) \
        dev_err(cmidid_device, "[%s:%d %s] Error: " pr_fmt(fmt), \
                        __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
