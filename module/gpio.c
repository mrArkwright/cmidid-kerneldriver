#include <linux/gpio.h>
#include <linux/interrupt.h>

static struct gpio gpios[] = {
	{4, GPIOF_IN, "Button 1"},
	{22, GPIOF_IN, "Button 2"}
};

static ktime_t button1_pressed;

static int irqs[ARRAY_SIZE(gpios)];

/*
 * Returns the index of the irq value in the irqs array.
 * Used to get the corresponding gpio from the gpios array.
 * Unfortunatly the function 'int irq_to_gpio(int irq)' in linux/gpio.h didn't work for me.
 */
static int get_irq_index(int irq)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		if (irqs[i] == irq)
			return i;
	}
	return -1;
}

/*
 * Interrupt service routine (ISR) for every GPIO port.
 */
static irqreturn_t gpio_isr(int irq, void *data)
{
	ktime_t diff;
	unsigned int velocity = 0;
	int i = get_irq_index(irq);
	if (gpios[i].gpio == 4 && button1_pressed.tv64 == 0) {
		button1_pressed = ktime_get();
		snd_printk(KERN_INFO LOGPREFIX
			   "Button 1 (GPIO %d) pressed. (time = %lld ns)\n",
			   gpios[i].gpio, button1_pressed.tv64);
	} else if (gpios[i].gpio == 22 && button1_pressed.tv64 != 0) {
		diff = ktime_sub(ktime_get(), button1_pressed);
		button1_pressed.tv64 = 0;

		velocity = diff.tv64 >> 24;
		if (velocity > 127)
			velocity = 127;
		velocity = 127 - velocity;
		send_note(0x3b, velocity);
		snd_printk(KERN_INFO LOGPREFIX
			   "Interrupt called: %d for gpio %d. (diff = %llu, velocity = %u)\n",
			   irq, gpios[i].gpio, diff.tv64, velocity);
	}
	return IRQ_HANDLED;
}

static int gpio_init(void)
{
	int i, ret;
	char portname[32];

	snd_printk(KERN_INFO LOGPREFIX "GPIO component initializing...\n");

	/* Initialize the irqs and irq_times arrays. */
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		irqs[i] = -1;
	}

	/* Request an array of GPIOs. */
	if ((ret = gpio_request_array(gpios, ARRAY_SIZE(gpios)))) {
		snd_printk(KERN_ERR LOGPREFIX
			   "Error. Unable to request GPIOs for 'gpios' array: %d\n",
			   ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		/* Request IRQs for every GPIO port. */
		if ((ret = gpio_to_irq(gpios[i].gpio)) < 0) {
			snd_printk(KERN_ERR LOGPREFIX
				   "Error. Unable to request IRQ for gpio: %d\n",
				   gpios[i].gpio);
			return ret;
		}
		snd_printk(KERN_INFO LOGPREFIX "Setting IRQ %d for gpio %d\n",
			   ret, gpios[i].gpio);
		irqs[i] = ret;
		/* Set the interrupt service routine for every port. */
		/* Add IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING. */
		sprintf(portname, "gpio %d", gpios[i].gpio);
		ret = request_irq(irqs[i], gpio_isr,
				  IRQF_TRIGGER_FALLING |
				  IRQF_DISABLED, portname, 0);
		if (ret) {
			snd_printk(KERN_ERR LOGPREFIX
				   "Unable to request IRQ for port %d\n",
				   gpios[i].gpio);
			return ret;
		}
	}

	return 0;
}

static void gpio_exit(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(irqs); i++) {
		free_irq(irqs[i], 0);
	}
	gpio_free_array(gpios, ARRAY_SIZE(gpios));

}
