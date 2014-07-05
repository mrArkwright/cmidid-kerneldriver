#include <linux/device.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/seq_kernel.h>
#include <sound/seq_virmidi.h>	/* TODO: Find real header for in_atomic. */

#include "cmidid_main.h"

struct snd_card *card;
static int client = 0;

static void config_note(struct snd_seq_event *note, unsigned char notevalue,
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
		if (err < 0) {
			warn("Error dispatch client(%d) code:%d\n", client,
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
