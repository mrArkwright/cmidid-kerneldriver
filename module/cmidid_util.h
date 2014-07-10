#ifndef CMIDID_UTIL_H
#define CMIDID_UTIL_H

#include <linux/device.h>

#define DEBUG

/*
 * Logging wrapper functions
 *
 * These macros print additional information like the
 * filename, line number and function name alongside
 * the message.
 */
extern struct device *cmidid_device;

#define info(fmt, ...) \
        dev_info(cmidid_device, "[%s] Info:" pr_fmt(fmt), \
                        __func__, ##__VA_ARGS__)

#define dbg(fmt, ...) \
        dev_dbg(cmidid_device, "[%s:%d %s] Debug: " pr_fmt(fmt), \
                        __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define warn(fmt, ...) \
        dev_warn(cmidid_device, "[%s] Warning" pr_fmt(fmt), \
                        __func__, ##__VA_ARGS__)

#define err(fmt, ...) \
        dev_err(cmidid_device, "[%s:%d %s] Error: " pr_fmt(fmt), \
                        __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
