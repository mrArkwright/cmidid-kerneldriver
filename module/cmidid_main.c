#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include "cmidid_main.h"
#include "cmidid_midi.h"
#include "cmidid_gpio.h"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Configurable Midi Driver");
MODULE_AUTHOR("Do you want to have your real names in the git history?");

static int __init cmidid_init(void);
static void __exit cmidid_exit(void);

module_init(cmidid_init);
module_exit(cmidid_exit);

static long cmidid_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations cdev_fops = {
	.unlocked_ioctl = cmidid_ioctl,
};

static char *cdev_name = IOCTL_DEV_NAME;
static unsigned int cdev_number = IOCTL_DEV_NUMBER;

static int __init cmidid_init(void)
{
	int err;

	info("Module initializing...\n");

	cdev_number = register_chrdev(cdev_number, cdev_name, &cdev_fops);

	if ((err = cdev_number) < 0) {
		err("%d. Could not register character device.\n", err);
		goto err_cdev_init;
	}

	info("Registered char device %s %d-0.\n", cdev_name, cdev_number);

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
	gpio_exit();

 err_midi_init:
	midi_exit();

 err_cdev_init:
	unregister_chrdev(cdev_number, cdev_name);
	info("Unregistered char device %s %d-0.\n", cdev_name, cdev_number);

	return err;
}

static void __exit cmidid_exit(void)
{
	info("Module exiting...\n");

	gpio_exit();
	midi_exit();

	unregister_chrdev(cdev_number, cdev_name);
	info("Unregistered char device %s %d-0.\n", cdev_name, cdev_number);
}

static long cmidid_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	info("`cmidid_ioctl' called with f=%p, cmd=%d, arg=%lu\n", f, cmd, arg);

	return 0;
}
