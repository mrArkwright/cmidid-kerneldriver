#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "cmidid_main.h"
#include "cmidid_midi.h"
#include "cmidid_gpio.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Custom Midi-Device Driver");
MODULE_AUTHOR
    ("Do you want to have your real names in the git history? -> yes! people should know that we're awesome.");

static int __init cmidid_init(void);
static void __exit cmidid_exit(void);

module_init(cmidid_init);
module_exit(cmidid_exit);

static long cmidid_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations cmidid_fops = {
	.unlocked_ioctl = cmidid_ioctl,
};

static dev_t cmidid_dev_number;
static struct cdev *cmidid_driver_object;
static struct class *cmidid_class;
static struct device *cmidid_devive;

static int __init cmidid_init(void)
{
	int err;

	info("Module initializing...\n");

	if (alloc_chrdev_region(&cmidid_dev_number, 0, 1, IOCTL_DEV_NAME) < 0)
		return -EIO;

	cmidid_driver_object = cdev_alloc();
	if (cmidid_driver_object == NULL) {
		err = -EIO;
		goto free_device_number;
	}
	cmidid_driver_object->owner = THIS_MODULE;
	cmidid_driver_object->ops = &cmidid_fops;

	if (cdev_add(cmidid_driver_object, cmidid_dev_number, 1)) {
		err = -EIO;
		goto free_cdev;
	}

	cmidid_class = class_create(THIS_MODULE, IOCTL_DEV_NAME);

	cmidid_devive =
	    device_create(cmidid_class, NULL, cmidid_dev_number, NULL, "%s",
			  IOCTL_DEV_NAME);

	if ((err = midi_init()) < 0) {
		err("%d. Could not initialize MIDI component.\n", err);
		goto err_midi_init;
	}

	if ((err = gpio_init()) < 0) {
		err("%d. Could not initialize GPIO component.\n", err);
		goto err_gpio_init;
	}

	return 0;

 err_gpio_init:
	midi_exit();

 err_midi_init:

 free_cdev:
	kobject_put(&cmidid_driver_object->kobj);

 free_device_number:
	unregister_chrdev_region(cmidid_dev_number, 1);

	return err;
}

static void __exit cmidid_exit(void)
{
	info("Module exiting...\n");

	gpio_exit();
	midi_exit();

	/* Delete Sysfs entry and device file  */
	device_destroy(cmidid_class, cmidid_dev_number);
	class_destroy(cmidid_class);
	/* Deletion of driver */
	cdev_del(cmidid_driver_object);
	unregister_chrdev_region(cmidid_dev_number, 1);

	info("Unregistered char device\n");
}

static long cmidid_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	info("`cmidid_ioctl' called with f=%p, cmd=%d, arg=%lu\n", f, cmd, arg);

	return 0;
}
