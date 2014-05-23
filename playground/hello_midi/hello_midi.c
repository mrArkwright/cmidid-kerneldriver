#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/rawmidi.h>
#include <linux/slab.h>

#include <sound/seq_virmidi.h>

void mylog(char *msg) {
	printk("hello_midi: %s\n", msg);
}

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

static int hello_midi_input_open(struct snd_rawmidi_substream *substream) {
	mylog("input open called");
	return 0;
}

static int hello_midi_input_close(struct snd_rawmidi_substream *substream) {
	mylog("input close called");
	return 0;
}

static void hello_midi_input_trigger(struct snd_rawmidi_substream *substream, int up) {
	mylog("input trigger called");
}

static struct snd_rawmidi_ops hello_midi_input_ops = {
	.open = &hello_midi_input_open,
	.close = &hello_midi_input_close,
	.trigger = &hello_midi_input_trigger,
};

static int hello_midi_output_open(struct snd_rawmidi_substream *substream) {
	mylog("output open called");
	return 0;
}

static int hello_midi_output_close(struct snd_rawmidi_substream *substream) {
	mylog("output close called");
	return 0;
}

static void hello_midi_output_trigger(struct snd_rawmidi_substream *substream, int up) {
	mylog("output trigger called");
}

static struct snd_rawmidi_ops hello_midi_output_ops = {
	.open = &hello_midi_output_open,
	.close = &hello_midi_output_close,
	.trigger = &hello_midi_output_trigger,
};

static int hello_midi_dev_register(struct snd_rawmidi *rmidi) {
	int err;
	struct snd_virmidi_dev *rdev;
	
	mylog("register called");
	
	rdev = rmidi->private_data;
	/*if ((err = snd_virmidi_dev_attach_seq(rdev)) < 0) {
		return err;
	}*/
	
	return 0;
}

static int hello_midi_dev_unregister(struct snd_rawmidi *rmidi) {
	struct snd_virmidi_dev *rdev;
	
	mylog("unregister called");
	
	/*rdev = rmidi->private_data;
	snd_virmidi_dev_detach_seq(rdev);*/
	
	return 0;
}

static struct snd_rawmidi_global_ops hello_midi_global_ops = {
	.dev_register = &hello_midi_dev_register,
	.dev_unregister = &hello_midi_dev_unregister,
};

static void hello_midi_free(struct snd_rawmidi *rmidi) {
	struct hello_midi_dev *rdev;
	
	mylog("free called");
	
	rdev = rmidi->private_data;
	kfree(rdev);
}

int hello_midi_new(struct snd_card *card, int device, struct snd_rawmidi **rrmidi) {
	struct snd_rawmidi *rmidi;
	struct snd_virmidi_dev *rdev;
	int err;

	*rrmidi = NULL;
	if ((err = snd_rawmidi_new(card, "VirMidi", device,
				   16,	/* may be configurable */
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
	rmidi->private_free = hello_midi_free;
	rmidi->ops = &hello_midi_global_ops;
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_INPUT, &hello_midi_input_ops);
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &hello_midi_output_ops);
	rmidi->info_flags = SNDRV_RAWMIDI_INFO_INPUT |
			    SNDRV_RAWMIDI_INFO_OUTPUT |
			    SNDRV_RAWMIDI_INFO_DUPLEX;
	*rrmidi = rmidi;
	return 0;
}

int hellomidi_probe(struct platform_device *devptr) {
	int err;
	struct snd_card *card;
	int dev = devptr->id;
	struct snd_rawmidi *rmidi;
	
	mylog("probe called");
	
	if ((err = snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card), &card)) < 0) {
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
	snd_rawmidi_set_ops(rmidi, SNDRV_RAWMIDI_STREAM_OUTPUT, &hello_midi_output_ops);*/
	
	hello_midi_new(card, 0, &rmidi);
	//snd_virmidi_new(card, 0, &rmidi);
	strcpy(rmidi->name, "hello midi");
	
	if ((err = snd_card_register(card)) < 0) {
		mylog("error registering card");
		return err;
	}
	
	platform_set_drvdata(devptr, card);
	
	return 0;
}

static struct platform_device *device;

int hellomidi_remove(struct platform_device *devptr) {
	mylog("remove called");
	
	snd_card_free(platform_get_drvdata(devptr));
	return 0;
}

static struct platform_driver hellomidi_driver = {
	.probe = &hellomidi_probe,
	.remove = &hellomidi_remove,
	.driver = {
		.name   = "hello_midi_f",
		.owner  = THIS_MODULE,
	},      
 };


int __init hello_midi_init(void) {
	mylog("init called");
	
	platform_driver_register(&hellomidi_driver);
	device = platform_device_register_simple("hello_midi_f", 0, NULL, 0);

	return 0;
}

void __exit hello_midi_exit(void) {
	mylog("exit called");

	platform_device_unregister(device);
	platform_driver_unregister(&hellomidi_driver);
}

module_init(hello_midi_init)
module_exit(hello_midi_exit)

MODULE_LICENSE("GPL");
