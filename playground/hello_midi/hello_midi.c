
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/rawmidi.h>
#include <linux/slab.h>

#include <sound/seq_virmidi.h>
#include <sound/initval.h>
#include <sound/seq_kernel.h>
#include <sound/seq_midi_event.h>
#include <sound/asoundef.h>
#include <sound/core.h>
#include <sound/core.h>
#include <sound/seq_device.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>

void mylog(char *msg)
{
	printk("hello_midi: %s\n", msg);
}

/* Define GPIOs for BUTTONS */
static struct gpio buttons[] = {
	{4, GPIOF_IN, "BUTTON 1"},	// turns LED on
	{22, GPIOF_IN, "BUTTON 2"},	// turns LED off
};
static int button_irqs[] = { -1, -1 };

static int client = 0;

struct hello_midi_dev {
	struct snd_card *card;
	struct snd_rawmidi *rmidi;
	int seq_mode;
	int device;
	int client;
	int port;
	unsigned int flags;
	rwlock_t filelist_lock;
	struct list_head filelist;
};

static int hello_midi_input_open(struct snd_rawmidi_substream *substream)
{
	mylog("input open called");
	return 0;
}

static int hello_midi_input_close(struct snd_rawmidi_substream *substream)
{
	mylog("input close called");
	return 0;
}

static void hello_midi_input_trigger(struct snd_rawmidi_substream *substream,
				     int up)
{
	mylog("input trigger called");
}

static struct snd_rawmidi_ops hello_midi_input_ops = {
	.open = &hello_midi_input_open,.close =
	    &hello_midi_input_close,.trigger = &hello_midi_input_trigger,
};

static int hello_midi_output_open(struct
				  snd_rawmidi_substream
				  *substream)
{
	mylog("output open called");
	return 0;
}

static int hello_midi_output_close(struct
				   snd_rawmidi_substream
				   *substream)
{
	mylog("output close called");
	return 0;
}

struct snd_rawmidi_substream *alsa_substream = NULL;
static void hello_midi_output_trigger(struct
				      snd_rawmidi_substream
				      *substream, int up)
{
	alsa_substream = substream;
	mylog("output trigger called");
}

void timercallback()
{
	if (client) {
		struct snd_seq_event testnote;
		testnote.type = SNDRV_SEQ_EVENT_NOTEON;
		testnote.flags =
		    SNDRV_SEQ_EVENT_LENGTH_FIXED | SNDRV_SEQ_PRIORITY_NORMAL;
		testnote.data.note.note = 0x3B;
		testnote.data.note.channel = 0x00;
		testnote.data.note.velocity = 0x64;
		testnote.data.note.duration = 0xffffff;
		testnote.data.note.off_velocity = 0x64;
		testnote.queue = SNDRV_SEQ_QUEUE_DIRECT;
		testnote.dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
		testnote.dest.port = 0;
		testnote.source.client = client;	// Dirty hack for testing
		testnote.source.port = 0;
		//struct snd_midi_event *evtptr = NULL;
		//int newret = snd_midi_event_new(10, &evtptr);
		//printk("new midi event ret:%d\n", newret);
		//if (newret == 0) {
		//char buf[3] = { 0x90, 0x3b, 0x64 };
		//struct snd_seq_event newevent[20];
		//int encret =
		//    snd_midi_event_encode(evtptr, buf, sizeof(buf),
		//                        &newevent[0]);
		//printk("Encode ret:%d", encret);

		int err = snd_seq_kernel_client_dispatch(client, &testnote,
							 in_atomic(), 0);
		printk("Hello midi client %d; dispatch ret:%d\n", client, err);
		//}
	}
}

static irqreturn_t button_isr(int irq, void *data)
{
	if (irq == button_irqs[0]) {
		printk(KERN_INFO "Current button1 value: %d\n",
		       gpio_get_value(buttons[0].gpio));
		timercallback();
	}

	return IRQ_HANDLED;
}

static struct snd_rawmidi_ops hello_midi_output_ops = {
	.open = &hello_midi_output_open,.close =
	    &hello_midi_output_close,.trigger = &hello_midi_output_trigger,
};

static int hello_midi_dev_register(struct
				   snd_rawmidi
				   *rmidi)
{
	struct snd_virmidi_dev *rdev;
	mylog("register called");
	rdev = rmidi->private_data;
	/*if ((err = snd_virmidi_dev_attach_seq(rdev)) < 0) {
	   return err;
	   } */
	return 0;
}

static int hello_midi_dev_unregister(struct
				     snd_rawmidi
				     *rmidi)
{
	/*struct snd_virmidi_dev *rdev; */

	/*rdev = rmidi->private_data;
	   snd_virmidi_dev_detach_seq(rdev); */
	if (client) {
		snd_seq_delete_kernel_client(client);
		client = 0;
	}
	mylog("unregister called");
	return 0;
}

static struct snd_rawmidi_global_ops hello_midi_global_ops = {
	.dev_register =
	    &hello_midi_dev_register,.dev_unregister =
	    &hello_midi_dev_unregister,
};

static void hello_midi_free(struct snd_rawmidi
			    *rmidi)
{
	struct hello_midi_dev *rdev;
	mylog("free called");
	rdev = rmidi->private_data;
	kfree(rdev);
}

void testcallback()
{
	mylog("Strange testcallback without arg evaluation\n");
}

static int snd_virmidi_dev_attach_seq(struct
				      snd_card
				      *card, int device)
{
	struct snd_seq_port_callback pcallbacks;
	struct snd_seq_port_info *pinfo;
	int err;
	pinfo = kzalloc(sizeof(*pinfo), GFP_KERNEL);
	if (!pinfo) {
		err = -ENOMEM;
		goto __error;
	}

	client = snd_seq_create_kernel_client(card, device, "hellomidiclient");
	if (client < 0) {
		mylog("client create error");
		err = client;
		goto __error;
	}

	/* create a port */
	pinfo->addr.client = client;
	sprintf(pinfo->name, "hellomidipinfo");
	/* set all capabilities */
	pinfo->capability |=
	    SNDRV_SEQ_PORT_CAP_WRITE |
	    SNDRV_SEQ_PORT_CAP_SYNC_WRITE | SNDRV_SEQ_PORT_CAP_SUBS_WRITE;
	pinfo->capability |=
	    SNDRV_SEQ_PORT_CAP_READ |
	    SNDRV_SEQ_PORT_CAP_SYNC_READ | SNDRV_SEQ_PORT_CAP_SUBS_READ;
	pinfo->capability |= SNDRV_SEQ_PORT_CAP_DUPLEX;
	pinfo->type =
	    SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC |
	    SNDRV_SEQ_PORT_TYPE_SOFTWARE | SNDRV_SEQ_PORT_TYPE_PORT;
	pinfo->midi_channels = 16;
	memset(&pcallbacks, 0, sizeof(pcallbacks));
	pcallbacks.owner = THIS_MODULE;
	pcallbacks.private_data = NULL;
	pcallbacks.subscribe = testcallback;
	pcallbacks.unsubscribe = testcallback;
	pcallbacks.use = testcallback;
	pcallbacks.unuse = testcallback;
	pcallbacks.event_input = testcallback;
	pinfo->kernel = &pcallbacks;
	err =
	    snd_seq_kernel_client_ctl(client,
				      SNDRV_SEQ_IOCTL_CREATE_PORT, pinfo);
	if (err < 0) {
		snd_seq_delete_kernel_client(client);
		//rdev->client = -1;
		goto __error;
	}
	//rdev->port = pinfo->addr.port;
	err = 0;		/* success */
 __error:
	kfree(pinfo);
	return err;
}

int hello_midi_new(struct snd_card *card,
		   int device, struct snd_rawmidi **rrmidi)
{
	struct snd_rawmidi *rmidi;
	struct snd_virmidi_dev *rdev;
	int err;
	printk(KERN_INFO "%s\n", __func__);
	*rrmidi = NULL;
	if ((err = snd_rawmidi_new(card, "VirMidi", device, 16,	/* may be configurable */
				   16,	/* may be configurable */
				   &rmidi))
	    < 0)
		return err;
	strcpy(rmidi->name, rmidi->id);
	rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
	if (rdev == NULL) {
		snd_device_free(card, rmidi);
		return -ENOMEM;
	}
	rdev->card = card;
	rdev->rmidi = rmidi;
	rdev->device = device;
	rdev->client = -1;
	rwlock_init(&rdev->filelist_lock);
	INIT_LIST_HEAD(&rdev->filelist);
	rdev->seq_mode = SNDRV_VIRMIDI_SEQ_DISPATCH;
	rmidi->private_data = rdev;
	rmidi->private_free = hello_midi_free;
	rmidi->ops = &hello_midi_global_ops;
	snd_rawmidi_set_ops(rmidi,
			    SNDRV_RAWMIDI_STREAM_INPUT, &hello_midi_input_ops);
	snd_rawmidi_set_ops(rmidi,
			    SNDRV_RAWMIDI_STREAM_OUTPUT,
			    &hello_midi_output_ops);
	rmidi->info_flags =
	    SNDRV_RAWMIDI_INFO_INPUT |
	    SNDRV_RAWMIDI_INFO_OUTPUT | SNDRV_RAWMIDI_INFO_DUPLEX;
	*rrmidi = rmidi;
	return 0;
}

int hellomidi_probe(struct platform_device
		    *devptr)
{
	int err;
	struct snd_card *card;
	int dev = devptr->id;
	struct snd_rawmidi *rmidi;
	mylog("probe called");
	if ((err =
	     snd_card_create(-1, NULL, THIS_MODULE,
			     sizeof(struct snd_card), &card)) < 0) {
		mylog("error creating card");
		return err;
	}

	strcpy(card->driver, "hello_midi_c");
	strcpy(card->shortname, "hello_midi_d");
	sprintf(card->longname, "hello_midi_e Card %i", dev + 1);
	/*if ((err = snd_rawmidi_new(card, "hello_midi_a", 0, 1, 1, &rmidi)) < 0) {
	   mylog("error rawmidi new");
	   return err;
	   }

	   strcpy(rmidi->name, "hello_midi_b");
	   rmidi->info_flags = SNDRV_RAWMIDI_INFO_OUTPUT;       
	   rmidi->ops = &hello_midi_global_ops;
	   //snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &hello_midi_input_ops);
	   snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &hello_midi_output_ops); */
	hello_midi_new(card, 0, &rmidi);
	//snd_virmidi_new(card, 0, &rmidi);
	strcpy(rmidi->name, "hello midi");
	snd_virmidi_dev_attach_seq(card, dev);
	snd_card_set_dev(card, &devptr->dev);
	if ((err = snd_card_register(card)) < 0) {
		mylog("error registering card");
		return err;
	}

	platform_set_drvdata(devptr, card);
	return 0;
}

static struct platform_device *device;
int hellomidi_remove(struct platform_device
		     *devptr)
{
	mylog("remove called");
	snd_card_free(platform_get_drvdata(devptr));
	return 0;
}

static struct platform_driver hellomidi_driver = {
	.probe = &hellomidi_probe,.remove = &hellomidi_remove,.driver = {
									 .name =
									 "hello_midi_f",.
									 owner =
									 THIS_MODULE,},
};

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */

static int gpiomode_init(void)
{
	int ret = 0;

	printk(KERN_INFO "%s\n", __func__);

	// register BUTTON gpios
	ret = gpio_request_array(buttons, ARRAY_SIZE(buttons));

	if (ret) {
		printk(KERN_ERR "Unable to request GPIOs for BUTTONs: %d\n",
		       ret);
		goto fail3;
	}

	printk(KERN_INFO "Current button1 value: %d\n",
	       gpio_get_value(buttons[0].gpio));

	ret = gpio_to_irq(buttons[0].gpio);

	if (ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	button_irqs[0] = ret;

	printk(KERN_INFO "Successfully requested BUTTON1 IRQ # %d\n",
	       button_irqs[0]);

	ret =
	    request_irq(button_irqs[0], button_isr,
			IRQF_TRIGGER_FALLING | IRQF_DISABLED, "gpiomod#button1",
			NULL);

	if (ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	ret = gpio_to_irq(buttons[1].gpio);

	if (ret < 0) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	button_irqs[1] = ret;

	printk(KERN_INFO "Successfully requested BUTTON2 IRQ # %d\n",
	       button_irqs[1]);

	ret =
	    request_irq(button_irqs[1], button_isr,
			IRQF_TRIGGER_FALLING | IRQF_DISABLED, "gpiomod#button2",
			NULL);

	if (ret) {
		printk(KERN_ERR "Unable to request IRQ: %d\n", ret);
		goto fail3;
	}

	return 0;

	// cleanup what has been setup so far
 fail3:
	free_irq(button_irqs[0], NULL);
	free_irq(button_irqs[1], NULL);
	return ret;
}

static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
int __init hello_midi_init(void)
{
	int i;
	mylog("init called");
	platform_driver_register(&hellomidi_driver);
	device = platform_device_register_simple("hello_midi_f", 0, NULL, 0);
	/*printk("SNDRV_CARDS: %d\n", SNDRV_CARDS);
	   for (i = 0; i < SNDRV_CARDS; i++) {
	   printk("SNDRV_DEFAULT_STR: %d\n", index[i]);
	   printk("SNDRV_DEFAULT_IDX: %s\n", id[i]);
	   } */ ;
	gpiomode_init();
	return 0;
}

void __exit hello_midi_exit(void)
{
	mylog("exit called");
	platform_device_unregister(device);
	platform_driver_unregister(&hellomidi_driver);
	free_irq(button_irqs[0], NULL);
	free_irq(button_irqs[1], NULL);
	gpio_free_array(buttons, ARRAY_SIZE(buttons));
}

module_init(hello_midi_init)
    module_exit(hello_midi_exit)

    MODULE_LICENSE("GPL");
