#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Raspberry Pi GPIO interface.");

const unsigned int PI_NUM_GPIOS = 26;

static int pi_gpio_init(void)
{
	int i;
	bool ret;

	printk(KERN_INFO "Raspberry Pi GPIO module initialized.");
	for (i = 0; i < PI_NUM_GPIOS; i++) {
		ret = gpio_is_valid(i);
		if (!ret)
			printk(KERN_INFO "Invalid GPIO: %d (%x)", i, i);
		printk(KERN_INFO "Testing GPIO: %d -> %s", i,
		       ret ? "true" : "false");
	}

	return 0;
}

static void pi_gpio_exit(void)
{
	printk("Raspberry Pi GPIO module exiting.");
}

module_init(pi_gpio_init);
module_exit(pi_gpio_exit);
