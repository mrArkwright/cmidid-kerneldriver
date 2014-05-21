#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/seq_kernel.h>
#include <sound/seq_virmidi.h>
#include <sound/initval.h>


struct snd_card_virmidi {
         struct snd_card *card;
         struct snd_rawmidi *midi[1];
};

void mylog(char *msg) {
	printk("hello_midi: %s\n", msg);
}

int hellomidi_probe(struct platform_device *devptr) {
	struct snd_card *card;
	int dev = devptr->id;
	int err;
	struct snd_rawmidi *rmidi;

	mylog("probe called");

	if ((err = snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card_virmidi), &card)) < 0) {
		mylog("error creating card");
		return err;
	}
	
	mylog("card created");
	
	snd_virmidi_new(card, 0, &rmidi);
	strcpy(rmidi->name, "hello midi");

	strcpy(card->driver, "helloMIDI");
	strcpy(card->shortname, "helloMIDI");
	sprintf(card->longname, "hello MIDI Card %i", dev + 1);

	if ((err = snd_card_register(card)) < 0) {
		mylog("error registering card");
		return err;
	}

	platform_set_drvdata(devptr, card);
	
	mylog("card registered");
	
	return 0;
}

static struct platform_device *device;

int hellomidi_remove(struct platform_device *devptr) {
	mylog("remove called");

	snd_card_free(platform_get_drvdata(devptr));
	return 0;
}

static struct platform_driver hellomidi_driver = {
          .probe          = hellomidi_probe,
          .remove         = hellomidi_remove,
          .driver         = {
                  .name   = "hello-midi",
                  .owner  = THIS_MODULE,
          },      
 };


int __init hello_midi_init(void) {
	mylog("init called");

	platform_driver_register(&hellomidi_driver);
	device = platform_device_register_simple("hello-midi", 0, NULL, 0);

	mylog("device registered");

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
