#ifndef CMIDID_MAIN_H
#define CMIDID_MAIN_H

#include <linux/kernel.h>

#define MODULE_NAME "cmidid"

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
 * Logging Settings
 *
 * You can use `info(...)' instead of `printk(KERN_INFO ...)'
 * and `err(...)' instead of `printk(KERN_ERR ...)'.
 *
 * The `err' macro is really useful, because it prints the filename,
 * line number and function name alongside the error message.
 **********************************************************************/

#define LOGPREFIX "[" MODULE_NAME "] "

#define info(fmt, ...) \
        printk(KERN_INFO LOGPREFIX "[%s] " pr_fmt(fmt), \
                        __func__, ##__VA_ARGS__)

#define err(fmt, ...) \
        printk(KERN_ERR LOGPREFIX "[%s:%d %s] Error: " pr_fmt(fmt), \
                        __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
