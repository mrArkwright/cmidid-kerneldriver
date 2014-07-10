/**
All alsa dependent initialisations and callbacks are handled here.
They are called by MIDIDriver abstraction layer.
*/

#include <linux/module.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/seq_kernel.h>
#include <sound/asoundef.h>
#include <linux/slab.h>

#include <linux/spinlock.h>

#include "rtpmidi.h"
#include "message.h"
#include "applemidi.h"
#include "clock.h"

#define SND_SEQ_EVENT_NOTEON 6
#define SND_SEQ_EVENT_NOTEOFF 7

/**
 * Struct to pass to the alsa callback with all necessary references
 */

struct privateData
{
	struct MIDIMessageList list;
	struct MIDIMessage msg;
	struct MIDIDriverAppleMIDI *drv;
};

struct ALSADriver
{
	struct snd_card *card;
	int client;
	struct privateData *priv;
};

static int _dummy_unuse(void *private_data, struct snd_seq_port_subscribe *info)
{
	return (0);
}

/*
 * event input callback - just redirect events to subscribers
 */
static int _alsa_input(struct snd_seq_event *ev, int direct, void *private_data,
		       int atomic, int hop)
{
	struct privateData *data = (struct privateData *)private_data;
	pr_debug("callback from alsa received of type %d\n", ev->type);

	spin_lock(&(data->drv->lock));

	if (ev->type == SND_SEQ_EVENT_NOTEON) {
		data->msg.data.bytes[0] = 0x90;
	} else if (ev->type == SND_SEQ_EVENT_NOTEOFF) {
		data->msg.data.bytes[0] = 0x80;
	} else {
		return 1;
	}
	
	//might be changed to ev->data.raw?? for event independant passing

	data->msg.data.bytes[0] |= ev->data.note.channel & 0xf;
	data->msg.data.bytes[1] = ev->data.note.note;
	data->msg.data.bytes[2] = ev->data.note.velocity;
	data->msg.data.bytes[3] = 0;
	data->msg.data.size = 3;
	data->msg.data.data = NULL;
	data->msg.format = MIDIMessageFormatDetect(&(data->msg.data.bytes[0]));
	pr_debug("found format\n");
	data->msg.timestamp = 0;

	RTPMIDISessionSend(data->drv->rtpmidi_session, &(data->list));

	spin_unlock(&(data->drv->lock));

	return 0;
}

/*
 * create a port
 */
static int _create_port(int my_client, int idx, int type, void *drv)
{

	struct snd_seq_port_info pinfo;
	struct snd_seq_port_callback pcb;

	pr_debug("register alsa port\n");
	memset(&pinfo, 0, sizeof(pinfo));
	pinfo.addr.client = my_client;

	sprintf(pinfo.name, "AppleMidi Port-%d", idx);

	pinfo.capability |=
	    SNDRV_SEQ_PORT_CAP_WRITE | SNDRV_SEQ_PORT_CAP_SUBS_WRITE;
	pinfo.type = SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC |
		     SNDRV_SEQ_PORT_TYPE_SOFTWARE | SNDRV_SEQ_PORT_TYPE_PORT;
	memset(&pcb, 0, sizeof(pcb));
	pcb.owner = THIS_MODULE;
	pcb.unuse = _dummy_unuse;
	pcb.event_input = _alsa_input;
	// pcb.private_free = dummy_free;
	pcb.private_data = drv;
	pinfo.kernel = &pcb;
	if (snd_seq_kernel_client_ctl(my_client, SNDRV_SEQ_IOCTL_CREATE_PORT,
				      &pinfo) < 0) {
		pr_err("could not register port\n");
		return -1;
	}
	return 0;
}

/*
 * register client and create port
 */
struct ALSADriver *ALSARegisterClient(void *drv)
{

	struct ALSADriver *driver;

	int ret, err;

	printk("registering alsa client\n");

	driver = kmalloc(sizeof(struct ALSADriver), GFP_KERNEL);

	driver->priv = kmalloc(sizeof(struct privateData), GFP_KERNEL);

	driver->priv->list.message = &(driver->priv->msg);
	driver->priv->list.next = NULL;
	driver->priv->drv = drv;

	err = snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card),
			      &(driver->card));
	if (err < 0) {
		pr_err("alsa error creating card: %d\n", err);
		goto fail_card;
	}

	/* create client */
	driver->client =
	    snd_seq_create_kernel_client(driver->card, 0, "AppleMidi");
	if (driver->client < 0) {
		pr_err("failed to register alsa client : %d\n", driver->client);
		goto fail_client;
	}

	/* create port */
	pr_debug("register alsa port\n");
	ret = _create_port(driver->client, 0, 0, driver->priv);
	if (ret < 0) {
		goto fail_port;
	}

	pr_debug("registering snd card\n");
	err = snd_card_register(driver->card);
	if (err < 0) {
		pr_err("failed to register card:err:%d\n", err);
		goto fail_port;
	}

	pr_debug("registered alsa client\n");

	return driver;

fail_port:
	snd_seq_delete_kernel_client(driver->client);

fail_client:
	snd_card_free(driver->card);

fail_card:
	kfree(driver->priv);
	kfree(driver);

	return (NULL);
}

/*
 * delete client if exists
 */
void ALSADeleteClient(struct ALSADriver *driver)
{
	if (driver == NULL) {
		return;
	}
	if (driver->client != 0) {
		snd_seq_delete_kernel_client(driver->client);
	}
	if (driver->card != NULL) {
		snd_card_free(driver->card);
	}
	if (driver->priv != NULL) {
		kfree(driver->priv);
	}
	kfree(driver);
	pr_debug("freed alsa port\n");
}
