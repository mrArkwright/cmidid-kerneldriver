#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "cmidid_main.h"
#include "cmidid_ioctl.h"
#include "cmidid_midi.h"
#include "cmidid_gpio.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Custom Midi-Device Driver");
MODULE_AUTHOR("Felix Engelmann, Michael Opitz, Andreas Ruhland, Jannik Theiß");

static int __init cmidid_init(void);
static void __exit cmidid_exit(void);

module_init(cmidid_init);
module_exit(cmidid_exit);

static long cmidid_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

/* We are only using ioctl and not read, write, etc. */
static struct file_operations cmidid_fops = {
	.unlocked_ioctl = cmidid_ioctl,
};

static dev_t cmidid_dev_number;
static struct cdev *cmidid_driver_object;
static struct class *cmidid_class;
struct device *cmidid_device;

/*
 * cmidid_inti: Init routine; called by the kernel.
 *
 * Return: An appropriate Linux error code.
 */
static int __init cmidid_init(void)
{
	int err;
	pr_debug("Module initializing...\n");

	if (alloc_chrdev_region(&cmidid_dev_number, 0, 1, IOCTL_DEV_NAME) < 0) {
		pr_err("error allocating character device region\n");
		return -EIO;
	}

	cmidid_driver_object = cdev_alloc();
	if (cmidid_driver_object == NULL) {
		pr_err("error allocating character device\n");
		err = -EIO;
		goto free_device_number;
	}
	cmidid_driver_object->owner = THIS_MODULE;
	cmidid_driver_object->ops = &cmidid_fops;

	if (cdev_add(cmidid_driver_object, cmidid_dev_number, 1)) {
		pr_err("error adding character device\n");
		err = -EIO;
		goto free_cdev;
	}

	cmidid_class = class_create(THIS_MODULE, IOCTL_DEV_NAME);

	cmidid_device =
	    device_create(cmidid_class, NULL, cmidid_dev_number, NULL, "%s",
			  IOCTL_DEV_NAME);
	if ((err = cmidid_midi_init()) < 0) {
		err("%d. Could not initialize MIDI component.\n", err);
		goto err_midi_init;
	}

	if ((err = gpio_init()) < 0) {
		err("%d. Could not initialize GPIO component.\n", err);
		goto err_gpio_init;
	}

	return 0;

/* Call exit/cleanup routines in reverse order. */
 err_gpio_init:
	cmidid_midi_exit();

 err_midi_init:
	device_destroy(cmidid_class, cmidid_dev_number);
	class_destroy(cmidid_class);
	cdev_del(cmidid_driver_object);

 free_cdev:
	kobject_put(&cmidid_driver_object->kobj);

 free_device_number:
	unregister_chrdev_region(cmidid_dev_number, 1);

	return err;
}

/*
 * cmidid_exit: Exit/cleanup routine; called by the kernel.
 *
 * Note: If cmidid_init fails, the module unloads directly and `cmidid_exit'
 * is never called.
 */
static void __exit cmidid_exit(void)
{
	dbg("Module exiting...\n");

	gpio_exit();
	cmidid_midi_exit();

	dbg("Unregistering char device\n");
	/* Delete Sysfs entry and device file  */
	device_destroy(cmidid_class, cmidid_dev_number);
	class_destroy(cmidid_class);
	/* Deletion of driver */
	cdev_del(cmidid_driver_object);
	unregister_chrdev_region(cmidid_dev_number, 1);
}

/*
 * cmidid_ioctl: ioctl callback function.
 *
 * @f: pointer to cmidid file; /dev/cmidid
 * @cmd: ioctl command encoded in a single byte.
 * @arg: unspecified number of additional arguments
 *
 * Returns: 0 if the command handling was successful;
 */
static long cmidid_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{

	dbg("ioctl called with: %d\n", cmd);
	switch (cmd) {
	case CMIDID_CALIBRATE_MIN_TIME:
		return set_min_stroke_time();
	case CMIDID_CALIBRATE_MAX_TIME:
		return set_max_stroke_time();
	case CMIDID_VEL_CURVE_LINEAR:
		set_vel_curve_linear();
		break;
	case CMIDID_VEL_CURVE_CONCAVE:
		set_vel_curve_concave();
		break;
	case CMIDID_VEL_CURVE_CONVEX:
		set_vel_curve_convex();
		break;
	case CMIDID_VEL_CURVE_SATURATED:
		set_vel_curve_saturated();
		break;
	case CMIDID_TRANSPOSE:
		return cmidid_transpose((signed char)arg) + 128;
		break;
	default:
		dbg("unknown ioctl command\n");
	}

	return 0;
}
