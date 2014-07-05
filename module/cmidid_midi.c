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

#include "cmidid_main.h"

static int cmidid_dev_register(struct snd_rawmidi *);
static int cmidid_dev_unregister(struct snd_rawmidi *);
static int cmididi_input_open(struct snd_rawmidi_substream *);
static int cmidid_input_close(struct snd_rawmidi_substream *);
static void cmidid_input_trigger(struct snd_rawmidi_substream *, int);
static int cmidid_output_open(struct snd_rawmidi_substream *);
static int cmidid_output_close(struct snd_rawmidi_substream *);
static void cmidid_output_trigger(struct snd_rawmidi_substream *, int);

int cmidid_probe(struct platform_device *);
int cmidid_remove(struct platform_device *);

static int client = 0;

static struct snd_rawmidi_global_ops cmidid_global_ops = {
	.dev_register = &cmidid_dev_register,
	.dev_unregister = &cmidid_dev_unregister,
};

static struct snd_rawmidi_ops cmidid_input_ops = {
	.open = &cmididi_input_open,
	.close = &cmidid_input_close,
	.trigger = &cmidid_input_trigger,
};

static struct snd_rawmidi_ops cmidid_output_ops = {
	.open = &cmidid_output_open,
	.close = &cmidid_output_close,
	.trigger = &cmidid_output_trigger,
};

static struct platform_driver cmidid_driver = {
	.probe = &cmidid_probe,
	.remove = &cmidid_remove,
	.driver = {
		   .name = "cmidid_f",
		   .owner = THIS_MODULE,},
};

static struct platform_device *device;

int cmidid_remove(struct platform_device
		  *devptr)
{
	printk(KERN_INFO LOGPREFIX "remove called\n");
	snd_card_free(platform_get_drvdata(devptr));
	return 0;
}

static int cmididi_input_open(struct snd_rawmidi_substream *substream)
{
	printk(KERN_INFO LOGPREFIX "input open\n");
	return 0;
}

static int cmidid_input_close(struct snd_rawmidi_substream *substream)
{
	printk(KERN_INFO LOGPREFIX "input close\n");
	return 0;
}

static void cmidid_input_trigger(struct snd_rawmidi_substream *substream,
				 int up)
{
	printk(KERN_INFO LOGPREFIX "input trigger\n");
}

static int cmidid_output_open(struct
			      snd_rawmidi_substream
			      *substream)
{
	printk(KERN_INFO LOGPREFIX "output open\n");
	return 0;
}

static int cmidid_output_close(struct
			       snd_rawmidi_substream
			       *substream)
{
	printk(KERN_INFO LOGPREFIX "output close\n");
	return 0;
}

static void cmidid_output_trigger(struct
				  snd_rawmidi_substream
				  *substream, int up)
{
	printk(KERN_INFO LOGPREFIX "output trigger\n");
}

static int cmidid_dev_register(struct
			       snd_rawmidi
			       *rmidi)
{
	struct snd_virmidi_dev *rdev;
	printk(KERN_INFO LOGPREFIX "register called\n");
	rdev = rmidi->private_data;
	return 0;
}

static int cmidid_dev_unregister(struct
				 snd_rawmidi
				 *rmidi)
{
	/*struct snd_virmidi_dev *rdev; */

	/*rdev = rmidi->private_data;
	   snd_virmidi_dev_detach_seq(rdev); */
	if (client > 0) {
		snd_seq_delete_kernel_client(client);
		client = 0;
	}
	printk(KERN_INFO LOGPREFIX "unregister called\n");
	return 0;
}

static void cmidid_free(struct snd_rawmidi
			*rmidi)
{
	printk(KERN_INFO LOGPREFIX "free called\n");
	/*rdev = rmidi->private_data;
	   kfree(rdev); */
}

void testcallback(void)
{
	printk(KERN_INFO LOGPREFIX
	       "testcallback without evaluation arguments\n");
}

/**
* Creates Sequencer for Soundcard. Copyed from seq_virmidi.c
*/
static int cmidid_dev_attach_seq(struct
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

	client = snd_seq_create_kernel_client(card, device, "cmidid-client");
	if (client < 0) {
		snd_printk(KERN_ERR LOGPREFIX "client create error\n");
		err = client;
		goto __error;
	}

	/* create a port */
	pinfo->addr.client = client;
	sprintf(pinfo->name, "cmididpinfo");
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
	pcallbacks.private_data = NULL;	/* FIXME: What should we store in private_data? */
	/* FIXME: Add appropriate callbacks if we want to react to subscribes etc. */
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
		client = 0;
		goto __error;
	}

	err = 0;		/* success */
 __error:
	kfree(pinfo);
	return err;
}

void config_note(struct snd_seq_event *note, unsigned char notevalue,
		 unsigned char velocity)
{
	if (notevalue <= 0)
		notevalue = 0;
	if (notevalue >= 127)
		notevalue = 127;
	if (velocity <= 0)
		velocity = 0;
	if (velocity >= 127)
		velocity = 127;

	note->type = SNDRV_SEQ_EVENT_NOTEON;
	note->flags = SNDRV_SEQ_EVENT_LENGTH_FIXED | SNDRV_SEQ_PRIORITY_NORMAL;
	note->data.note.note = notevalue;
	note->data.note.channel = 0x00;
	note->data.note.velocity = velocity;
	note->data.note.duration = 0xffffff;
	note->data.note.off_velocity = 0x64;
	note->queue = SNDRV_SEQ_QUEUE_DIRECT;
	note->dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
	note->dest.port = 0;	/* FIXME: Which ports to use ? */
	note->source.client = client;
	note->source.port = 0;
}

void send_note(unsigned char notevalue, unsigned char velocity)
{
	struct snd_seq_event note;
	int err;

	if (client > 0) {
		config_note(&note, notevalue, velocity);
		err =
		    snd_seq_kernel_client_dispatch(client, &note, in_atomic(),
						   0);
		if (err < 0)
			snd_printk(KERN_ERR LOGPREFIX
				   "Error dispatch client(%d) code:%d\n",
				   client, err);
	}
}

int cmidid_new(struct snd_card *card, int device, struct snd_rawmidi **rrmidi)
{
	struct snd_rawmidi *rmidi;
	struct snd_virmidi_dev *rdev;
	int err;
	printk(KERN_INFO LOGPREFIX "%s\n", __func__);
	*rrmidi = NULL;
	if ((err = snd_rawmidi_new(card, "CMidiD", device, 16,	/* may be configurable */
				   16,	/* may be configurable */
				   &rmidi)) < 0)
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
	rmidi->private_free = cmidid_free;
	rmidi->ops = &cmidid_global_ops;
	snd_rawmidi_set_ops(rmidi,
			    SNDRV_RAWMIDI_STREAM_INPUT, &cmidid_input_ops);
	snd_rawmidi_set_ops(rmidi,
			    SNDRV_RAWMIDI_STREAM_OUTPUT, &cmidid_output_ops);
	rmidi->info_flags =
	    SNDRV_RAWMIDI_INFO_INPUT |
	    SNDRV_RAWMIDI_INFO_OUTPUT | SNDRV_RAWMIDI_INFO_DUPLEX;
	*rrmidi = rmidi;
	return 0;
}

int cmidid_probe(struct platform_device
		 *devptr)
{
	int err;
	struct snd_card *card;
	int dev = devptr->id;
	struct snd_rawmidi *rmidi;

	if ((err =
	     snd_card_create(-1, NULL, THIS_MODULE,
			     sizeof(struct snd_card), &card)) < 0) {
		snd_printk(KERN_ERR LOGPREFIX "error creating card\n");
		return err;
	}

	strcpy(card->driver, "cmidid_c");
	strcpy(card->shortname, "cmidid_d");
	sprintf(card->longname, "cmidid_e Card %i", dev + 1);

	if ((err = cmidid_new(card, 0, &rmidi)) < 0)
		goto __nodev;

	strcpy(rmidi->name, "cmidid rawmidi");

	snd_card_set_dev(card, &devptr->dev);

	if ((err = snd_card_register(card)) == 0) {
		platform_set_drvdata(devptr, card);

		cmidid_dev_attach_seq(card, dev);	/* Perhaps move to register function */

		return 0;
	}

	snd_printk(KERN_ERR LOGPREFIX "error registering card\n");

 __nodev:
	snd_card_free(card);
	return err;
}

int midi_init(void)
{
	printk(KERN_INFO LOGPREFIX "MIDI component initializing...\n");
	platform_driver_register(&cmidid_driver);
	device = platform_device_register_simple("cmidid_f", 0, NULL, 0);
	return 0;
}

void midi_exit(void)
{
	platform_device_unregister(device);
	platform_driver_unregister(&cmidid_driver);
}
