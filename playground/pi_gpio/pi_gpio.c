#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/gpio.h>
#include <arch/arm/bcm2708/gpio.h>
#include <linux/interrupt.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Raspberry Pi GPIO interface.");

struct gpio ports[] = {
	{4, GPIOF_IN, "BUTTON 1"},
	{22, GPIOF_IN, "BUTTON 2"}
};

int port_irqs[ARRAY_SIZE(ports)];

ktime_t irq_times[ARRAY_SIZE(ports)];

/*
 * Returns the index of the irq value in the port_irqs array.
 * This can be used to get the gpio or irq_time for the ports and irq_times arrays.
 * Unfortunatly the function 'int irq_to_gpio(int irq)' in linux/gpio.h didn't work for me.
 */
static int get_irq_index(int irq)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(port_irqs); i++) {
		if (port_irqs[i] == irq)
			return i;
	}
	return -1;
}

/*
 * Interrupt service routine (ISR) for every GPIO port.
 */
static irqreturn_t pi_gpio_isr(int irq, void *data)
{
	int gpio = irq_to_gpio(irq);
	irq_times[i] = ktime_get();
	ktime_sub(ktime_get(), irq_times[i]);
	printk(KERN_INFO
	       "[pi_gpio:%s] interrupt called: %d for gpio %d. (diff = %lld ns)\n",
	       __func__, irq, ports[i].gpio, irq_times[i].tv64);

	return IRQ_HANDLED;
}

/*
 * Free the IRQs here, so that other modules can access the GPIOs.
 */
static void pi_gpio_cleanup(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(port_irqs); i++) {
		free_irq(port_irqs[i], 0);
	}
	gpio_free_array(ports, ARRAY_SIZE(ports));
}

/*
 * Module init function.
 * Initialized the required arrays and requests the IRQs for every GPIO.
 */
static int pi_gpio_init(void)
{
	int i, ret;
	char portname[32];

	printk(KERN_INFO
	       "[pi_gpio:%s] Raspberry Pi GPIO module initializing...",
	       __func__);

	/* Initialize the port_irqs and irq_times arrays. */
	for (i = 0; i < ARRAY_SIZE(ports); i++) {
		port_irqs[i] = -1;
		irq_times[i].tv64 = 0;
	}

	/* Request an array of GPIOs. */
	if ((ret = gpio_request_array(ports, ARRAY_SIZE(ports)))) {
		printk(KERN_ERR
		       "[pi_gpio:%s] Unable to request GPIOs for 'ports' array: %d\n",
		       __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ports); i++) {
		/* Request IRQs for every GPIO port. */
		if ((ret = gpio_to_irq(ports[i].gpio)) < 0) {
			printk(KERN_ERR
			       "[pi_gpio:%s] Unable to request IRQ for port: %d\n",
			       __func__, ports[i].gpio);
			return ret;
		}
		printk(KERN_INFO "[pi_gpio:%s] Setting IRQ %d for port %d\n",
		       __func__, ret, ports[i].gpio);
		port_irqs[i] = ret;
		/* Set the interrupt service routine for every port. */
		/* Add IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING. */
		sprintf(portname, "pi_gpio:port %d", ports[i].gpio);
		ret =
		    request_irq(port_irqs[i], pi_gpio_isr,
				IRQF_TRIGGER_FALLING |
				IRQF_DISABLED, portname, 0);
		if (ret) {
			printk(KERN_ERR
			       "[pi_gpio:%s] Unable to request IRQ for port %d\n",
			       __func__, ports[i].gpio);
			return ret;
		}

	}

	for (i = 0; i < ARRAY_SIZE(irq_times); i++) {
		irq_times[i] = ktime_get();
	}

	return 0;
}

/*
 * Module exit function.
 * pi_gpio_cleanup must be called here.
 */
static void pi_gpio_exit(void)
{
	printk("Raspberry Pi GPIO module exiting.");
	pi_gpio_cleanup();
}

module_init(pi_gpio_init);
module_exit(pi_gpio_exit);
