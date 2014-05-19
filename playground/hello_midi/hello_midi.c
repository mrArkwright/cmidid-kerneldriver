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

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;      /* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR; 



int hellomidi_probe(struct platform_device *devptr) {
	struct snd_card *card;
	int dev = devptr->id;
	int err;
	struct snd_rawmidi *rmidi;

	err = snd_card_create(index[dev], id[dev], THIS_MODULE, sizeof(struct snd_card_virmidi), &card);
	
	printk("creating card %i\n",err);
	
	snd_virmidi_new(card, 0, &rmidi);
	strcpy(rmidi->name, "hello midi");

	strcpy(card->driver, "helloMIDI");
	strcpy(card->shortname, "helloMIDI");
	sprintf(card->longname, "hello MIDI Card %i", dev + 1);

	err=  snd_card_register(card);
	
	printk("register card %i\n",err);
	
	return 0;
}

static struct platform_device *devices[SNDRV_CARDS];

int hellomidi_remove(struct platform_device *devptr) {
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


int init_module(void) {
	//int i,cards;
	
	printk("init called\n");
	platform_driver_register(&hellomidi_driver);
	
	/*cards=0;
	for (i = 0; i < SNDRV_CARDS; i++) {
		*/
		struct platform_device *device;
		/*if (! i==3)
			continue;
		*/
		device = platform_device_register_simple("hello-midi",
							 3, NULL, 0);
		printk("reged dev");
		/*if (IS_ERR(device))
			continue;
		if (!platform_get_drvdata(device)) {
			platform_device_unregister(device);
			continue;
		}
		printk("reged ready");
		*/devices[3] = device;
	/*	cards++;
	}
	if (!cards) {
             printk(KERN_ERR "Card-VirMIDI soundcard not found or device busy\n");

	}*/	
	return 0;
}

void cleanup_module(void) {
	platform_driver_unregister(&hellomidi_driver);
	printk("cleanup_module called\n");
}

MODULE_LICENSE("GPL");
