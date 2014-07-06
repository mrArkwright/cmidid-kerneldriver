#define DEBUG

#include <linux/device.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/seq_kernel.h>
#include <sound/seq_virmidi.h>	/* TODO: Find real header for in_atomic. */

#include "cmidid_main.h"

struct snd_card *card;
static int client = 0;

static void config_note_event(struct snd_seq_event *event, unsigned char note,
			      unsigned char velocity,
			      snd_seq_event_type_t type);
static void dispatch_event(struct snd_seq_event *event);

void note_on(unsigned char note, unsigned char velocity)
{
	struct snd_seq_event event;

	dbg("noteon note: %d, vel: %d\n", note, velocity);

	config_note_event(&event, note, velocity, SNDRV_SEQ_EVENT_NOTEON);
	dispatch_event(&event);
}

void note_off(unsigned char note)
{
	struct snd_seq_event event;

	dbg("noteoff note: %d\n", note);

	config_note_event(&event, note, 64, SNDRV_SEQ_EVENT_NOTEOFF);
	dispatch_event(&event);
}

static void config_note_event(struct snd_seq_event *event, unsigned char note,
			      unsigned char velocity, snd_seq_event_type_t type)
{
	if (note <= 0)
		note = 0;
	if (note >= 127)
		note = 127;
	if (velocity <= 0)
		velocity = 0;
	if (velocity >= 127)
		velocity = 127;

	event->type = type;
	event->flags = SNDRV_SEQ_EVENT_LENGTH_FIXED | SNDRV_SEQ_PRIORITY_NORMAL;
	event->data.note.note = note;
	event->data.note.channel = 0x00;
	event->data.note.velocity = velocity;
	event->data.note.duration = 0xffffff;
	event->data.note.off_velocity = 0x64;
	event->queue = SNDRV_SEQ_QUEUE_DIRECT;
	event->dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
	event->dest.port = 0;	/* FIXME: Which ports to use ? */
	event->source.client = client;
	event->source.port = 0;
}

static void dispatch_event(struct snd_seq_event *event)
{
	int err;

	if (client > 0) {
		err =
		    snd_seq_kernel_client_dispatch(client, event, in_atomic(),
						   0);

		if (err < 0) {
			warn("couldn't dispatch note(%d) code:%d\n", client,
			     err);
		}
	}
}

int midi_init(void)
{
	int err;
	struct snd_seq_port_info *pinfo;

	err =
	    snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card),
			    &card);
	if (err < 0) {
		err("error creating card: %d\n", err);
		return err;
	}

	client = snd_seq_create_kernel_client(card, 0, "cmidid");
	if (client < 0) {
		err("error creating client: %d\n", err);
		return err;
	}

	pinfo = kzalloc(sizeof(struct snd_seq_port_info), GFP_KERNEL);
	if (!pinfo) {
		err = -ENOMEM;
		return err;
	}
	pinfo->addr.client = client;
	pinfo->capability |=
	    SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SYNC_READ |
	    SNDRV_SEQ_PORT_CAP_SUBS_READ;

	err =
	    snd_seq_kernel_client_ctl(client, SNDRV_SEQ_IOCTL_CREATE_PORT,
				      pinfo);
	if (err < 0) {
		err("error creating port: %d\n", err);
		return err;
	}

	return 0;
}

void midi_exit(void)
{
	snd_seq_delete_kernel_client(client);
	snd_card_free(card);
}
