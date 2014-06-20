#include <linux/module.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/time.h>
#include <net/sock.h>

#include <linux/slab.h>

/* #include "rtp.h" */

typedef long long MIDITimestamp;


struct AppleMIDICommand {
	struct RTPPeer *peer;	/* use peers sockaddr instead .. we get
				 * initialization problems otherwise */
	struct sockaddr_storage addr;
	socklen_t	size;
	/* unsigned short channel; */
	unsigned short	type;
	union {
		struct {
			unsigned long	version;
			unsigned long	token;
			unsigned long	ssrc;
			char		name      [64];
		}		session;
		struct {
			unsigned long	ssrc;
			unsigned long	count;
			unsigned long long timestamp1;
			unsigned long long timestamp2;
			unsigned long long timestamp3;
		}		sync;
		struct {
			unsigned long	ssrc;
			unsigned long	seqnum;
		}		feedback;
	}		data;
};

struct MIDIDriverAppleMIDI {
	//struct MIDIDriver base;
	struct socket  *control_socket;
	struct socket  *rtp_socket;
	unsigned short	port;
	unsigned char	accept;
	unsigned char	sync;
	unsigned long	token;
	char		name      [32];

	struct AppleMIDICommand command;

	struct RTPPeer *peer;
	struct RTPSession *rtp_session;
	struct RTPMIDISession *rtpmidi_session;

	struct MIDIMessageQueue *in_queue;
	struct MIDIMessageQueue *out_queue;
};


struct MIDIDriverAppleMIDI *raspi;

static int
_applemidi_connect(struct MIDIDriverAppleMIDI *driver)
{
	struct sockaddr_in addr;
	int		result = 0;

	if (driver->control_socket == NULL) {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(driver->port);

		if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &driver->control_socket) < 0) {
			printk(KERN_ERR "server: Error creating controlsocket\n");
			return -EIO
		}
		result = driver->control_socket->ops->bind(driver->control_socket, (struct sockaddr *)&addr, sizeof(addr));
		if (result) {
			goto control_fail;
		}
	}
	if (driver->rtp_socket == NULL) {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(driver->port + 1);

		if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &driver->rtp_socket) < 0) {
			printk(KERN_ERR "server: Error creating datasocket\n");
			goto control_fail;
		}
		result = driver->control_socket->ops->bind(driver->rtp_socket, (struct sockaddr *)&addr, sizeof(addr));
		if (result) {
			goto rtp_fail;
		}
	}
rtp_fail:
	sock_release(driver->rtp_socket);
	driver->rtp_socket = NULL;

control_fail:
	sock_release(driver->control_socket);
	driver->control_socket = NULL;

	return result;
}

static int 
_applemidi_disconnect(struct MIDIDriverAppleMIDI *driver, struct socket *sock)
{
	/*
	 * struct RTPPeer * peer = NULL; RTPSessionNextPeer(
	 * driver->rtp_session, &peer ); while( peer != NULL ) {
	 * _applemidi_disconnect_peer( driver, peer ); peer = NULL; /* peer
	 * was removed, find the new first
	 */
	/*
	 * RTPSessionNextPeer( driver->rtp_session, &peer ); }
	 */
	if (sock == driver->control_socket || sock == NULL) {
		//MIDIRunloopSourceClearRead(driver->base.rls, driver->control_socket);
		//MIDIRunloopSourceClearWrite(driver->base.rls, driver->control_socket);
		if (driver->control_socket != NULL) {
			sock_release(driver->control_socket);
			driver->control_socket = NULL;
		}
	}
	if (sock == driver->rtp_socket || sock == NULL) {
		//MIDIRunloopSourceClearRead(driver->base.rls, driver->rtp_socket);
		//MIDIRunloopSourceClearWrite(driver->base.rls, driver->rtp_socket);
		if (driver->rtp_socket != NULL) {
			sock_release(driver->rtp_socket);
			driver->rtp_socket = NULL;
		}
	}
	return 0;
}

/**
 * @brief Create a MIDIDriverAppleMIDI instance.
 * Allocate space and initialize an MIDIDriverAppleMIDI instance.
 * @public @memberof MIDIDriverAppleMIDI
 * @return a pointer to the created driver structure on success.
 * @return a @c NULL pointer if the driver could not created.
 */
struct MIDIDriverAppleMIDI *
MIDIDriverAppleMIDICreate(char *name, unsigned short port)
{
	struct MIDIDriverAppleMIDI *driver;
	MIDITimestamp	timestamp;

	driver = kmalloc(sizeof(struct MIDIDriverAppleMIDI), GFP_KERNEL);
	if (driver == NULL) {
		return -ENOMEM;
	}
	//MIDIDriverInit(&(driver->base), name, APPLEMIDI_CLOCK_RATE);

	driver->control_socket = NULL;
	driver->rtp_socket = NULL;
	driver->port = port;
	driver->accept = 0;
	driver->sync = 0;
	strncpy(&(driver->name[0]), name, sizeof(driver->name));

	//driver->in_queue = MIDIMessageQueueCreate();
	//driver->out_queue = MIDIMessageQueueCreate();

	//driver->base.send = &_driver_send;
	//driver->base.destroy = &_driver_destroy;

	_applemidi_connect(driver);

	/*
	 * driver->peer = NULL; driver->rtp_session =
	 * RTPSessionCreate(driver->rtp_socket); driver->rtpmidi_session =
	 * RTPMIDISessionCreate(driver->rtp_session);
	 * 
	 * MIDIClockGetNow(driver->base.clock, &timestamp); MIDILog(DEBUG,
	 * "initial timestamp: %lli\n", timestamp); driver->token =
	 * timestamp;
	 * 
	 * memset(&(driver->command), 0, sizeof(driver->command));
	 * driver->command.peer = NULL;
	 * 
	 * _applemidi_init_runloop_source(driver);
	 */
	return driver;
}

/**
 * @brief Destroy a MIDIDriverAppleMIDI instance.
 * Free all resources occupied by the driver.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 */
void 
MIDIDriverAppleMIDIDestroy(struct MIDIDriverAppleMIDI *driver)
{
	_applemidi_disconnect(driver, NULL);
	//RTPMIDISessionRelease(driver->rtpmidi_session);
	//RTPSessionRelease(driver->rtp_session);
	//MIDIMessageQueueRelease(driver->in_queue);
	//MIDIMessageQueueRelease(driver->out_queue);
	free(driver);
}


static int	__init
mod_init(void)
{
	raspi = MIDIDriverAppleMIDICreate("kernel", 5008);
}

static void	__exit
mod_exit(void)
{
	MIDIDriverAppleMIDIDestroy(raspi);
}


module_init(mod_init);
module_exit(mod_exit);
