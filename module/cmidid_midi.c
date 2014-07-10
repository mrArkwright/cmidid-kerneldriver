#define DEBUG

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>

#include <sound/core.h>
#include <sound/seq_kernel.h>
#include <sound/seq_virmidi.h>	/* TODO: Find real header for in_atomic. */

#include "cmidid_main.h"

/*
 * The midi channel which is used to send notes.
 */
static char midi_channel = 0x00;
module_param(midi_channel, byte, 0);
MODULE_PARM_DESC(midi_channel, "Which midi channel to use (0 - 15).");

struct cmidid_midi_state {
	struct snd_card *card;
	int client;
	char midi_channel;
	signed char transpose;
};

static struct cmidid_midi_state state = {
	.card = NULL,
	.client = 0,
	.midi_channel = 0x00,
	.transpose = 0
};

static void config_note_event(struct snd_seq_event *event, unsigned char note,
			      unsigned char velocity,
			      snd_seq_event_type_t type);
static void dispatch_event(struct snd_seq_event *event);

/**
* cmidid_transpose: 
* 
* Reachable form userspace with an ioctl call. It will add
* the give value to the traspose
*	
* @arg: the change ot the transposed value
*
* Return: the new absolute transpose value between -128 and 127 
**/
signed char cmidid_transpose(signed char transpose)
{
	state.transpose += transpose;

	dbg("transpose by: %d, new transpose: %d\n", transpose,
	    state.transpose);

	return state.transpose;
}

/*
* note_on:  send note on to midi device
*
* @note: the note to be sended between 0 and 127
* @velocity: the velocity of this note
*/
void cmidid_note_on(unsigned char note, unsigned char velocity)
{
	struct snd_seq_event event;

	dbg("noteon note: %d, vel: %d\n", note, velocity);

	config_note_event(&event, note, velocity, SNDRV_SEQ_EVENT_NOTEON);
	dispatch_event(&event);
}

/*
* note_off: send note off to midi device. For every note on one
*           note off should be sended.
*
* @note: send note of to
*/
void cmidid_note_off(unsigned char note)
{
	struct snd_seq_event event;

	dbg("noteoff note: %d\n", note);

	config_note_event(&event, note, 127, SNDRV_SEQ_EVENT_NOTEOFF);
	dispatch_event(&event);
}

/*
* config_note_event: configure a alsa seq event as note
*
* @event: this event will be filled with information
* @note: th note which sould be set. Forced bounds between 0 and 127
* @velocity: Velocity of the note. Forced bounds between 0 and 127
* @type: note on or note off event
*/
static void config_note_event(struct snd_seq_event *event, unsigned char note,
			      unsigned char velocity, snd_seq_event_type_t type)
{
	note += state.transpose;

	if (note >= 127)
		note = 127;
	if (velocity >= 127)
		velocity = 127;

	event->type = type;
	event->flags = SNDRV_SEQ_EVENT_LENGTH_FIXED | SNDRV_SEQ_PRIORITY_NORMAL;
	event->data.note.note = note;
	event->data.note.channel = state.midi_channel;
	event->data.note.velocity = velocity;
	event->data.note.duration = 0xffffff;
	event->data.note.off_velocity = 0x64;
	event->queue = SNDRV_SEQ_QUEUE_DIRECT;
	event->dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
	event->dest.port = 0;	/* FIXME: Which ports to use ? */
	event->source.client = state.client;
	event->source.port = 0;
}

static void dispatch_event(struct snd_seq_event *event)
{
	int err;

	if (state.client > 0) {
		err =
		    snd_seq_kernel_client_dispatch(state.client, event,
						   in_atomic(), 0);
		if (err < 0) {
			warn("couldn't dispatch note(%d) code:%d\n",
			     state.client, err);
		}
	}
}

/*
* midi_init: Initialize midi component
* 
* Return: zero means success and negative value error
*/
int cmidid_midi_init(void)
{
	int err;
	struct snd_seq_port_info *pinfo;

	if (midi_channel < 0x00 || midi_channel > 0x0F) {
		err("Midi channel must be between 0 and 15\n");
		err = -EINVAL;
		return err;
	}

	state.midi_channel = midi_channel;

	err =
	    snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card),
			    &state.card);
	if (err < 0) {
		err("error creating card: %d\n", err);
		return err;
	}

	state.client = snd_seq_create_kernel_client(state.card, 0, "cmidid");
	if (state.client < 0) {
		err("error creating client: %d\n", err);
		return err;
	}

	/* TODO: free pinfo */
	pinfo = kzalloc(sizeof(struct snd_seq_port_info), GFP_KERNEL);
	if (!pinfo) {
		err = -ENOMEM;
		return err;
	}
	pinfo->addr.client = state.client;
	pinfo->capability |=
	    SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SYNC_READ |
	    SNDRV_SEQ_PORT_CAP_SUBS_READ;

	err =
	    snd_seq_kernel_client_ctl(state.client, SNDRV_SEQ_IOCTL_CREATE_PORT,
				      pinfo);
	if (err < 0) {
		err("error creating port: %d\n", err);
		return err;
	}

	return 0;
}

/*
* midi_exit: cleanup midi component
*/
void cmidid_midi_exit(void)
{
	snd_seq_delete_kernel_client(state.client);
	snd_card_free(state.card);
}
