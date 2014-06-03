#include <linux/ktime.h>

static int time_init(void)
{
	snd_printk(KERN_INFO LOGPREFIX "Time component initializing...\n");
	return 0;
}

static void time_exit(void)
{
	snd_printk(KERN_INFO LOGPREFIX "Time component exiting...\n");
}
