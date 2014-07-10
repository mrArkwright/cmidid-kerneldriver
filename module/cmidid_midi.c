#define DEBUG

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>

#include <linux/hardirq.h>	// for in_atomic()

#include <sound/core.h>
#include <sound/seq_kernel.h>

#include "cmidid_main.h"

/*
 * The midi channel which is used for the generated notes.
 */
static char midi_channel = 0x00;
module_param(midi_channel, byte, 0);
MODULE_PARM_DESC(midi_channel, "Which midi channel to use (0 - 15).");

/*
 * cmidid_midi_state:
 *
 * The state of the MIDI component of our kernel module.
 * @card: The sound card registered to the system.
 * @client: The client number used in the alsa sequencer system.
 * @midi_channel: the midi_channel used for the generated notes
 * @transpose: the transpose value in semitones added to the tone pitch.
 */
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

/*
 * cmidid_transpose: Add a value to the current transpose.
 *
 * @transpose: the value in semitones added to the current transpose
 *
 * return: the new absolute transpose value in semitones (between -128 and 127)
 */
signed char cmidid_transpose(signed char transpose)
{
	state.transpose += transpose;

	dbg("transpose by: %d, new transpose: %d\n", transpose,
	    state.transpose);

	return state.transpose;
}

/*
* cmidid_note_on: Trigger a note_on event.
*
* @note: the pitch of the note (between 0 and 127)
* @velocity: the velocity of the note
*/
void cmidid_note_on(unsigned char note, unsigned char velocity)
{
	struct snd_seq_event event;

	dbg("noteon note: %d, vel: %d\n", note, velocity);

	config_note_event(&event, note, velocity, SNDRV_SEQ_EVENT_NOTEON);
	dispatch_event(&event);
}

/*
 * cmidid_note_off: Trigger a note_off event. For every note_on a
 * note_off should be triggered.
 *
 * @note: the pitch of the note to turn off
 */
void cmidid_note_off(unsigned char note)
{
	struct snd_seq_event event;

	dbg("noteoff note: %d\n", note);

	config_note_event(&event, note, 127, SNDRV_SEQ_EVENT_NOTEOFF);
	dispatch_event(&event);
}

/*
 * config_note_event: Configure a alsa sequencer event as note.
 * The 
 *
 * @event: a pointer to the event which will be configured
 * @note: the note which sould be set. Forced bounds between 0 and 127
 * @velocity: Velocity of the note. Forced bounds between 0 and 127
 * @type: note on or note off event
 */
static void config_note_event(struct snd_seq_event *event, unsigned char note,
			      unsigned char velocity, snd_seq_event_type_t type)
{
	//take transpose into account
	note += state.transpose;

	//cap the pitch of the note at 127
	if (note >= 127)
		note = 127;

	//cap the velocity of the note at 127
	if (velocity >= 127)
		velocity = 127;

	event->type = type;
	event->flags = SNDRV_SEQ_EVENT_LENGTH_FIXED | SNDRV_SEQ_PRIORITY_NORMAL;
	event->data.note.note = note;
	event->data.note.channel = state.midi_channel;
	event->data.note.velocity = velocity;
	event->data.note.duration = 0xffffff;
	event->data.note.off_velocity = velocity;
	event->queue = SNDRV_SEQ_QUEUE_DIRECT;
	event->dest.client = SNDRV_SEQ_ADDRESS_SUBSCRIBERS;
	event->dest.port = 0;	/* FIXME: Which ports to use ? */
	event->source.client = state.client;
	event->source.port = 0;
}

/*
 * dispatch_event: dispatch an alsa event to the alsa
 * sequencer client registered by this module
 *
 * @event: the event to dispatch
 */
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
 * cmidid_midi_init: Initialize MIDI component.
 * 
 * return: zero on success and negative error code
 * on error
 */
int cmidid_midi_init(void)
{
	int err;
	struct snd_seq_port_info pinfo;

	//check if the midi_channel set module param is in valid range (0 - 15)
	if (midi_channel < 0x00 || midi_channel > 0x0F) {
		err("Midi channel must be between 0 and 15\n");
		err = -EINVAL;
		return err;
	}
	//copy midi_channel to state struct
	state.midi_channel = midi_channel;

	// register sound card at the alsa system
	err =
	    snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card),
			    &state.card);
	if (err < 0) {
		err("error creating card: %d\n", err);
		return err;
	}
	// add sequencer client to our sound card
	state.client = snd_seq_create_kernel_client(state.card, 0, "cmidid");
	if (state.client < 0) {
		err("error creating client: %d\n", err);
		return err;
	}
	// configure our sequencer client to have one readable (output) port
	memset(&pinfo, 0, sizeof(struct snd_seq_port_info));
	pinfo.addr.client = state.client;
	pinfo.capability |=
	    SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SYNC_READ |
	    SNDRV_SEQ_PORT_CAP_SUBS_READ;

	err =
	    snd_seq_kernel_client_ctl(state.client, SNDRV_SEQ_IOCTL_CREATE_PORT,
				      &pinfo);
	if (err < 0) {
		err("error creating port: %d\n", err);
		return err;
	}

	return 0;
}

/*
 * cmidid_midi_exit: Cleanup MIDI component.
 */
void cmidid_midi_exit(void)
{
	// free our sequencer client
	snd_seq_delete_kernel_client(state.client);

	// free our sound card
	snd_card_free(state.card);
}
