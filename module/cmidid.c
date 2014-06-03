#include <linux/kernel.h>
#include <linux/module.h>
#include <sound/core.h>

#define LOGPREFIX "[cmidid] "

#include "time.c"
#include "midi.c"
#include "gpio.c"

int __init cmidid_init(void)
{
	int ret;

	snd_printk(KERN_INFO LOGPREFIX "Module initializing...\n");

	if ((ret = time_init()) < 0)
		goto err_time_init;
	if ((ret = midi_init()) < 0)
		goto err_midi_init;
	if ((ret = gpio_init()) < 0)
		goto err_gpio_init;

	return 0;

 err_gpio_init:
	gpio_exit();
 err_midi_init:
	midi_exit();
 err_time_init:
	time_exit();

	return ret;
}

void __exit cmidid_exit(void)
{
	snd_printk(KERN_INFO LOGPREFIX "Module exiting...\n");
	gpio_exit();
	midi_exit();
	time_exit();
}

module_init(cmidid_init);
module_exit(cmidid_exit);

MODULE_LICENSE("GPL");
