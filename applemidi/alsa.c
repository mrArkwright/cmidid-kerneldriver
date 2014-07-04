#include <linux/module.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/seq_kernel.h>
#include <sound/asoundef.h>
#include <linux/slab.h>

#include "rtpmidi.h"
#include "message.h"
#include "applemidi.h"
#include "clock.h"

#define SND_SEQ_EVENT_NOTEON 6
#define SND_SEQ_EVENT_NOTEOFF 7

struct privateData{
	struct MIDIMessageList list;
	struct MIDIMessage msg;
	struct MIDIDriverAppleMIDI *drv;
};

struct ALSADriver{
	struct snd_card *card;
	int client;
	struct privateData *priv;
};

static int
_dummy_unuse(void *private_data, struct snd_seq_port_subscribe *info)
{
	return(0);
}

/*
 * event input callback - just redirect events to subscribers
 */
static int
_alsa_input(struct snd_seq_event *ev, int direct, void *private_data,
	    int atomic, int hop)
{
	struct privateData *data= (struct privateData *)private_data;
	printk("callback from alsa received of type %d\n",ev->type);
	
	//data->list[0].message=;
    
    if(ev->type == SND_SEQ_EVENT_NOTEON)
    {
        data->msg.data.bytes[0]=0x90;
    }
    else if(ev->type == SND_SEQ_EVENT_NOTEOFF)
    {
        data->msg.data.bytes[0]=0x80;
    }
    else
    {
        return 1;
    }
    
    
	data->msg.data.bytes[0] |= ev->data.note.channel & 0xf;
	data->msg.data.bytes[1]=ev->data.note.note;
	data->msg.data.bytes[2]=ev->data.note.velocity;
	data->msg.data.bytes[3]=0;
	data->msg.data.size=3;
	data->msg.data.data=NULL;
	data->msg.format=MIDIMessageFormatDetect(&(data->msg.data.bytes[0]));
	printk("found format\n");
	data->msg.timestamp=0;
		
	
	RTPMIDISessionSend( data->drv->rtpmidi_session, &(data->list) );
	
	return 0;
	
}


/*
 * create a port
 */
static int
_create_port(int my_client, int idx, int type, void *drv)
{
	
	struct snd_seq_port_info pinfo;
	struct snd_seq_port_callback pcb;
	
	printk("register alsa port\n");
	//struct snd_seq_dummy_port *rec;

	/*if ((rec = kzalloc(sizeof(*rec), GFP_KERNEL)) == NULL)
		return NULL;

	rec->client = my_client;
	rec->duplex = duplex;
	rec->connect = 0;
	*/
	memset(&pinfo, 0, sizeof(pinfo));
	pinfo.addr.client = my_client;
	/*if (duplex)
		sprintf(pinfo.name, "Midi Through Port-%d:%c", idx,
			(type ? 'B' : 'A'));
	else*/
		
	sprintf(pinfo.name, "AppleMidi Port-%d", idx);
	
	//pinfo.capability = SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SUBS_READ;
	pinfo.capability |= SNDRV_SEQ_PORT_CAP_WRITE | SNDRV_SEQ_PORT_CAP_SUBS_WRITE;
	//if (duplex)
	//	pinfo.capability |= SNDRV_SEQ_PORT_CAP_DUPLEX;
	pinfo.type = SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC
		| SNDRV_SEQ_PORT_TYPE_SOFTWARE
		| SNDRV_SEQ_PORT_TYPE_PORT;
	memset(&pcb, 0, sizeof(pcb));
	pcb.owner = THIS_MODULE;
	pcb.unuse = _dummy_unuse;
	pcb.event_input = _alsa_input;
	//pcb.private_free = dummy_free;
	pcb.private_data = drv;
	pinfo.kernel = &pcb;
	if (snd_seq_kernel_client_ctl(my_client, SNDRV_SEQ_IOCTL_CREATE_PORT, &pinfo) < 0) {
		//kfree(rec);
		printk("could not register port\n");
		return -1;
	}
	//rec->port = pinfo.addr.port;
	return 0;
}

/*
 * register client and create ports
 */
struct ALSADriver *
ALSARegisterClient(void * drv)
{
	
	struct ALSADriver *driver;
	//struct privateData *data;
	
	//struct snd_seq_dummy_port *rec1, *rec2;
	//int i;
	
	int ret,err;
	
	
	printk("registering alsa client\n");
	
	driver=kmalloc(sizeof(struct ALSADriver), GFP_KERNEL);
	
	driver->priv= kmalloc(sizeof(struct privateData), GFP_KERNEL);
		
	driver->priv->list.message=&(driver->priv->msg);
	driver->priv->list.next=NULL;
	driver->priv->drv=drv;
	
	
	//int my_client=-1;

	//struct snd_card *card;
	
	err = snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card), &(driver->card));
	if (err < 0) {
		printk("alsa error creating card: %d\n", err);
		goto fail_card;
	}
	

	//if (ports < 1) {
	//	snd_printk(KERN_ERR "invalid number of ports %d\n", ports);
	//	return -EINVAL;
	//}

	/* create client */
	driver->client = snd_seq_create_kernel_client(driver->card, 0,
						 "AppleMidi");
	if (driver->client < 0)
	{	
		printk("failed to register alsa client : %d\n",driver->client);
		goto fail_client;
	}
	
	
	/* create ports */
	//for (i = 0; i < ports; i++) {
	printk("register alsa port\n");
	ret = _create_port(driver->client, 0, 0, driver->priv);
	if (ret < 0) {
		//snd_seq_delete_kernel_client(driver->client);
		goto fail_port;
		//	return NULL;
	}
	//	if (duplex) {
	//		rec2 = create_port(i, 1);
	//		if (rec2 == NULL) {
	//			snd_seq_delete_kernel_client(my_client);
	//			return -ENOMEM;
	//		}
	//		rec1->connect = rec2->port;
	//		rec2->connect = rec1->port;
	//	}
	//}
	
	printk("registering snd card\n");
	err = snd_card_register(driver->card);
	if (err < 0) {
		printk("failed to register card:err:%d\n",err);
		goto fail_port;
		//  snd_card_free(driver->card);
        //return NULL;
    }
	
	printk("registered alsa client\n");
	
	return driver;
	
	fail_port:
		snd_seq_delete_kernel_client(driver->client);
	
	fail_client:
		snd_card_free(driver->card);
	
	fail_card:
		kfree(driver->priv);
		kfree(driver);
		
	
	return(NULL);
	
}

/*
 * delete client if exists
 */
void
ALSADeleteClient(struct ALSADriver *driver)
{
	if(driver == NULL)
	{
		return;
	}
	if (driver->client != 0)
	{
		snd_seq_delete_kernel_client(driver->client);
	}
	if (driver->card != NULL)
	{
		snd_card_free(driver->card);
	}
	if (driver->priv != NULL)
	{
		kfree(driver->priv);
	}
	kfree(driver);	
	printk("freed alsa port\n");
}

