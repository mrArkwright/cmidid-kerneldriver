/**
 * This manager handles the main logic of the applemidi driver.
 * It handles the reception of network packets an the complete 
 * session management. 
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <net/sock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "applemidi.h"
#include "rtp.h"
#include "rtpmidi.h"
#include "midi.h"
#include "clock.h"

struct MIDIDriverAppleMIDI *raspi;

int port = 5008;

module_param(port, int, 0);

/**
 * @brief Send the given AppleMIDI command.
 * Compose a message buffer and send the datagram to the given peer.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param fd The file descriptor to use for communication.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_send_command(struct MIDIDriverAppleMIDI *driver,
				   struct sock *sk,
				   struct AppleMIDICommand *command)
{
	unsigned int ssrc;
	unsigned int msg[16];
	int len, sentbytes = 0;

	struct sockaddr_in *a;
	struct sockaddr_in to;

	struct socket *source = NULL;

	mm_segment_t oldfs;
	struct msghdr udpmsg;
	struct iovec iov;

	msg[0] = htonl((APPLEMIDI_PROTOCOL_SIGNATURE << 16) | command->type);
	switch (command->type) {
	case APPLEMIDI_COMMAND_INVITATION:
	case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
	case APPLEMIDI_COMMAND_INVITATION_REJECTED:
	case APPLEMIDI_COMMAND_ENDSESSION:
		ssrc = command->data.session.ssrc;
		msg[1] = htonl(command->data.session.version);
		msg[2] = htonl(command->data.session.token);
		msg[3] = htonl(command->data.session.ssrc);
		if (command->data.session.name[0] != '\0') {
			len = strlen(command->data.session.name) + 1;
			memcpy(&(msg[4]), command->data.session.name, len);
			len += 16;
		} else {
			len = 16;
		}
		pr_debug("built session packet, ready to send\n");
		break;
	case APPLEMIDI_COMMAND_SYNCHRONIZATION:
		ssrc = command->data.sync.ssrc;
		msg[1] = htonl(command->data.sync.ssrc);
		msg[2] = htonl(command->data.sync.count << 24);
		msg[3] = htonl(command->data.sync.timestamp1 >> 32);
		msg[4] = htonl(command->data.sync.timestamp1 & 0xffffffff);
		msg[5] = htonl(command->data.sync.timestamp2 >> 32);
		msg[6] = htonl(command->data.sync.timestamp2 & 0xffffffff);
		msg[7] = htonl(command->data.sync.timestamp3 >> 32);
		msg[8] = htonl(command->data.sync.timestamp3 & 0xffffffff);
		len = 36;
		break;
	case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
		ssrc = command->data.feedback.ssrc;
		msg[1] = htonl(command->data.feedback.ssrc);
		msg[2] = htonl(command->data.feedback.seqnum);
		len = 12;
		break;
	default:
		return 1;
	}

	if (command->addr.sin_family == AF_INET) {
		a = (struct sockaddr_in *)&(command->addr);
		pr_debug("send %i bytes to %pI4:%i on s(%p)\n", len,
			 &a->sin_addr, ntohs(a->sin_port), sk);

		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_addr.s_addr = a->sin_addr.s_addr;
		to.sin_port = a->sin_port;

		udpmsg.msg_name = &to;
		udpmsg.msg_namelen = sizeof(to);
		iov.iov_base = (char *)&msg[0];
		iov.iov_len = len;
		udpmsg.msg_control = NULL;
		udpmsg.msg_controllen = 0;
		udpmsg.msg_iov = &iov;
		udpmsg.msg_iovlen = 1;
		if (driver->control_socket->sk == sk) {
			source = driver->control_socket;
			pr_debug("send by control socket\n");
		} else if (driver->rtp_socket->sk == sk) {
			source = driver->rtp_socket;
			pr_debug("send by rtp socket\n");
		}
		if (source != NULL) {
			oldfs = get_fs();
			set_fs(KERNEL_DS);
			sentbytes = sock_sendmsg(source, &udpmsg, len);
			set_fs(oldfs);
		}

		pr_debug("sent %d of %d bytes\n", sentbytes, len);

		if (sentbytes != len) {
			return 1;
		} else {
			return 0;
		}
	}
	return 1;
}

/**
 * @brief Receive an AppleMIDI command.
 * Receive a datagram and decompose the message into the message structure.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param fd The file descriptor to use for communication.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_recv_command(struct MIDIDriverAppleMIDI *driver,
				   struct sk_buff *skb,
				   struct AppleMIDICommand *command)
{
	unsigned int ssrc;
	unsigned int msg[16];
	int len, i;

	struct iphdr *iph;
	u16 *sp, *l;

	command->size = sizeof(command->addr);

	l = (u16 *)(skb->data + 4);
	len = ntohs(*l) - 8;

	memcpy(&msg[0], skb->data + 8, len);

	iph = (struct iphdr *)skb_network_header(skb);

	command->addr.sin_addr.s_addr = iph->saddr;
	command->addr.sin_family = AF_INET;
	sp = (unsigned short *)(skb->data);
	command->addr.sin_port = *sp;

	pr_debug("received pkt from %pI4:%d\n", &command->addr.sin_addr.s_addr,
		 ntohs(*sp));
	for (i = 0; i < 16; i++) {
		pr_debug("%x ", msg[i]);
	}
	pr_debug("\n");

	command->type = ntohl(msg[0]) & 0xffff;

	switch (command->type) {
	case APPLEMIDI_COMMAND_INVITATION:
	case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
	case APPLEMIDI_COMMAND_INVITATION_REJECTED:
	case APPLEMIDI_COMMAND_ENDSESSION:
		if (len < 16)
			return 1;
		command->data.session.version = ntohl(msg[1]);
		command->data.session.token = ntohl(msg[2]);
		command->data.session.ssrc = ntohl(msg[3]);
		len -= 16;
		if (len > 0) {
			if (len > sizeof(command->data.session.name) - 1) {
				len = sizeof(command->data.session.name) - 1;
			}
			memcpy(&(command->data.session.name[0]), &msg[4], len);
			command->data.session.name[len] = '\0';
		}
		ssrc = command->data.session.ssrc;

		pr_debug(
		    "found session pkt: %c%c , ver:%lu,token:%lu,ssrc:%lu, "
		    "name:%s\n",
		    command->type >> 8, command->type & 0xff,
		    command->data.session.version, command->data.session.token,
		    command->data.session.ssrc, command->data.session.name);
		break;
	case APPLEMIDI_COMMAND_SYNCHRONIZATION:
		if (len != 36)
			return 1;
		command->data.sync.ssrc = ntohl(msg[1]);
		command->data.sync.count = ntohl(msg[2]) >> 24;
		command->data.sync.timestamp1 =
		    (unsigned long long)ntohl(msg[3]) << 32;
		command->data.sync.timestamp1 += ntohl(msg[4]);
		command->data.sync.timestamp2 =
		    (unsigned long long)ntohl(msg[5]) << 32;
		command->data.sync.timestamp2 += ntohl(msg[6]);
		command->data.sync.timestamp3 =
		    (unsigned long long)ntohl(msg[7]) << 32;
		command->data.sync.timestamp3 += ntohl(msg[8]);
		ssrc = command->data.sync.ssrc;
		pr_debug("found sync pkt: %c%c , ssrc:%lu, cnt=%lu, t1=%llu, "
			 "t2=%llu, t3=%llu \n",
			 command->type >> 8, command->type & 0xff,
			 command->data.sync.ssrc, command->data.sync.count,
			 command->data.sync.timestamp1,
			 command->data.sync.timestamp2,
			 command->data.sync.timestamp3);
		break;
	case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
		if (len != 12)
			return 1;
		command->data.feedback.ssrc = ntohl(msg[1]);
		command->data.feedback.seqnum = ntohl(msg[2]);
		ssrc = command->data.feedback.ssrc;
		break;
	default:
		return 1;
	}
	return 0;
}

/**
 * @brief Start or continue a synchronization session.
 * Continue a synchronization session identified by a given command.
 * The command must contain a pointer to a valid peer.
 * @param driver The driver.
 * @param fd The file descriptor to be used for communication.
 * @param command The previous sync command.
 * @retval 0 On success.
 * @retval >0 If the synchronization failed.
 */
static int _applemidi_sync(struct MIDIDriverAppleMIDI *driver, struct sock *sk,
			   struct AppleMIDICommand *command)
{
	unsigned long ssrc;
	MIDITimestamp timestamp, diff;
	RTPSessionGetSSRC(driver->rtp_session, &ssrc);
	MIDIClockGetNow(driver->base.clock, &timestamp);
	pr_debug("got timestamp %lld\n", timestamp);
	if (command->type != APPLEMIDI_COMMAND_SYNCHRONIZATION ||
	    command->data.sync.ssrc == ssrc || command->data.sync.count > 2) {
		command->type = APPLEMIDI_COMMAND_SYNCHRONIZATION;
		command->data.sync.ssrc = ssrc;
		command->data.sync.count = 0;
		command->data.sync.timestamp1 = timestamp;

		driver->sync = 1;
		return _applemidi_send_command(driver, sk, command);
	} else {
		RTPSessionFindPeerBySSRC(driver->rtp_session, &(driver->peer),
					 command->data.sync.ssrc);

		/* received packet from other peer */
		if (command->data.sync.count == 2) {
			/* compute media delay */
			diff = (command->data.sync.timestamp3 -
				command->data.sync.timestamp1) /
			       2;
			/* approximate time difference between peer and self */
			diff = command->data.sync.timestamp3 + diff - timestamp;

			/* RTPPeerSetTimestampDiff( command->peer, diff ) */
			/* finished sync */
			command->data.sync.ssrc = ssrc;
			command->data.sync.count = 3;

			driver->sync = 0;
			return 0;
		}
		if (command->data.sync.count == 1) {
			/* compute media delay */
			diff = (command->data.sync.timestamp3 -
				command->data.sync.timestamp1) /
			       2;
			/* approximate time difference between peer and self */
			diff = command->data.sync.timestamp2 + diff - timestamp;

			/* RTPPeerSetTimestampDiff( command->peer, diff ) */

			command->data.sync.ssrc = ssrc;
			command->data.sync.count = 2;
			command->data.sync.timestamp3 = timestamp;

			driver->sync = 0;
			return _applemidi_send_command(driver, sk, command);
		}
		if (command->data.sync.count == 0) {
			command->data.sync.ssrc = ssrc;
			command->data.sync.count = 1;
			command->data.sync.timestamp2 = timestamp;

			driver->sync = 2;
			return _applemidi_send_command(driver, sk, command);
		}
	}
	return 1;
}

static int _applemidi_start_sync(struct MIDIDriverAppleMIDI *driver,
				 struct sock *sk, int size,
				 struct sockaddr_in *addr)
{
	if (addr != &(driver->command.addr))
		memcpy(&(driver->command.addr), addr, size);
	driver->command.size = size;
	driver->command.type = APPLEMIDI_COMMAND_SYNCHRONIZATION;
	driver->command.data.sync.count = 3;
	RTPSessionGetSSRC(driver->rtp_session,
			  &(driver->command.data.sync.ssrc));

	return _applemidi_sync(driver, sk, &(driver->command));
}

/**
 * @brief Respond to a given AppleMIDI command.
 * Use the command as response and - if neccessary - send it back to the peer.
 * @private @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 * @param fd The file descriptor to be used for communication.
 * @param command The command.
 * @retval 0 On success.
 * @retval >0 If the packet could not be sent.
 */
static int _applemidi_respond(struct MIDIDriverAppleMIDI *driver,
			      struct sock *sk, struct AppleMIDICommand *command)
{
	struct RTPPeer *peer = NULL;

	switch (command->type) {
	case APPLEMIDI_COMMAND_INVITATION:
		if (sk == driver->control_socket->sk) {
			// event = MIDIEventCreate(
			// MIDI_APPLEMIDI_PEER_DID_SEND_INVITATION, NULL, "%s",
			// &(command->data.session.name[0]) );
			// MIDIDriverTriggerEvent( &(driver->base), event );
			// MIDIEventRelease( event );
		}
		if (driver->accept) {
			command->type = APPLEMIDI_COMMAND_INVITATION_ACCEPTED;
			if (sk == driver->rtp_socket->sk) {
				peer = RTPPeerCreate(
				    command->data.session.ssrc, command->size,
				    (struct sockaddr_in *)&(command->addr));
				RTPSessionAddPeer(driver->rtp_session, peer);
				RTPPeerRelease(peer);
			}
		} else {
			command->type = APPLEMIDI_COMMAND_INVITATION_REJECTED;
		}
		RTPSessionGetSSRC(driver->rtp_session,
				  &(command->data.session.ssrc));
		pr_debug("processed invitation should now answer\n");
		return _applemidi_send_command(driver, sk, command);
	case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
		// TODO for receive
		break;
	case APPLEMIDI_COMMAND_INVITATION_REJECTED:
		// TODO for receive
		break;
	case APPLEMIDI_COMMAND_ENDSESSION:
		RTPSessionFindPeerBySSRC(driver->rtp_session, &peer,
					 command->data.session.ssrc);
		// event = MIDIEventCreate( MIDI_APPLEMIDI_PEER_DID_END_SESSION,
		// NULL, "%s", &(command->data.session.name[0]) );
		// MIDIDriverTriggerEvent( &(driver->base), event );
		// MIDIEventRelease( event );
		if (peer != NULL) {
			RTPSessionRemovePeer(driver->rtp_session, peer);
		}
		break;
	case APPLEMIDI_COMMAND_SYNCHRONIZATION:
		return _applemidi_sync(driver, sk, command);
	case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
		// RTPSessionFindPeerBySSRC( driver->rtp_session, &peer,
		// command->data.feedback.ssrc );
		// RTPMIDISessionJournalTrunkate( driver->rtpmidi_session, peer,
		// command->data.feedback.seqnum );
		break;
	default:
		return 1;
	}
	return 0;
}

static int _test_applemidi(u16 *buf, int len)
{
	if (len < 4)
		return -1;
	if (ntohs(buf[0]) == APPLEMIDI_PROTOCOL_SIGNATURE) {
		switch (ntohs(buf[1])) {
		case APPLEMIDI_COMMAND_INVITATION:
		case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
		case APPLEMIDI_COMMAND_INVITATION_REJECTED:
		case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
		case APPLEMIDI_COMMAND_SYNCHRONIZATION:
		case APPLEMIDI_COMMAND_ENDSESSION:
			return 0;
		}
	}
	return 1;
}

/**
 * @brief Receive Packets from Socket
 * This callback is called from the network stack
 * @private @memberof MIDIDriverAppleMIDI
 * @param sk The socket.
 * @param bytes The number of bytes received.
 */

static void _socket_callback(struct sock *sk, int bytes)
{
	struct MIDIDriverAppleMIDI *driver = raspi; // should NOT be global

	int len = 0;
	int length;
	u16 *l;

	struct sk_buff *skb;

	pr_debug("received %d bytes\n", bytes);

	len = skb_queue_len(&sk->sk_receive_queue);
	while (len > 0) {
		skb = skb_dequeue(&sk->sk_receive_queue);

		l = (u16 *)(skb->data + 4);
		length = ntohs(*l);
		if (length >= (8 + 4)) {
			if (_test_applemidi((u16 *)(skb->data + 8),
					    length - 8) == 0) {
				pr_debug("is applemidi message\n");
				// test if possible now:
				if (spin_trylock(&(driver->lock))) {
					pr_debug("allowed (lock not taken)\n");
					if (_applemidi_recv_command(
						driver, skb,
						&(driver->command)) == 0) {
						pr_debug("reply to command\n");
						_applemidi_respond(
						    driver, sk,
						    &(driver->command));
					}
					spin_unlock(&(driver->lock));
				} else {
					pr_warn("dropped packet because of "
						"sending note\n");
				}
			} else if (driver->rtp_socket->sk == sk) {
				// handle incomming midi
				//_applemidi_receive_rtpmidi( driver );
			}
		}

		len--;
		kfree_skb(skb);
	}
}

static int _applemidi_connect(struct MIDIDriverAppleMIDI *driver)
{
	struct sockaddr_in addr;
	int result = 0;

	if (driver->control_socket == NULL) {
		pr_debug("connect control\n");
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(driver->port);

		if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP,
				&driver->control_socket) < 0) {
			pr_err(KERN_ERR
			       "server: Error creating controlsocket\n");
			return -EIO;
		}
		result = driver->control_socket->ops->bind(
		    driver->control_socket, (struct sockaddr *)&addr,
		    sizeof(addr));
		if (result) {
			goto control_fail;
		}

		driver->control_socket->sk->sk_data_ready = _socket_callback;

		pr_debug("control ready\n");
	}
	if (driver->rtp_socket == NULL) {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(driver->port + 1);

		if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP,
				&driver->rtp_socket) < 0) {
			pr_err(KERN_ERR "server: Error creating datasocket\n");
			goto control_fail;
		}
		result = driver->control_socket->ops->bind(
		    driver->rtp_socket, (struct sockaddr *)&addr, sizeof(addr));
		if (result) {
			goto rtp_fail;
		}

		driver->rtp_socket->sk->sk_data_ready = _socket_callback;
		pr_debug("rtp ready\n");
	}

	return result;

rtp_fail:
	sock_release(driver->rtp_socket);
	driver->rtp_socket = NULL;

control_fail:
	sock_release(driver->control_socket);
	driver->control_socket = NULL;

	return result;
}

static int _applemidi_endsession(struct MIDIDriverAppleMIDI *driver, struct sock *sk,
				 int size, struct sockaddr_in *addr)
{
	if (addr != (struct sockaddr_in *)&(driver->command.addr))
		memcpy(&(driver->command.addr), addr, size);
	driver->command.size = size;
	driver->command.type = APPLEMIDI_COMMAND_ENDSESSION;
	driver->command.data.session.version = 2;
	driver->command.data.session.token = driver->token;
	RTPSessionGetSSRC(driver->rtp_session,
			  &(driver->command.data.session.ssrc));
	strncpy(&(driver->command.data.session.name[0]), driver->name,
		sizeof(driver->command.data.session.name));

	return _applemidi_send_command(driver, sk, &(driver->command));
}

static int _applemidi_disconnect_peer(struct MIDIDriverAppleMIDI *driver,
				      struct RTPPeer *peer)
{
	int result = 0;
	struct sockaddr_in *rtp_addr = NULL;
	int size;
	if (RTPPeerGetAddress(peer, &size, &rtp_addr) || rtp_addr == NULL) {
		return 1;
	}
	result = _applemidi_endsession(driver, driver->control_socket->sk, size,
				       (struct sockaddr_in *)&rtp_addr);
	RTPSessionRemovePeer(driver->rtp_session, peer);
	return result;
}

static int _applemidi_disconnect(struct MIDIDriverAppleMIDI *driver,
				 struct socket *sock)
{
	struct RTPPeer *peer = NULL;
	RTPSessionNextPeer(driver->rtp_session, &peer);
	while (peer != NULL) {
		_applemidi_disconnect_peer(driver, peer);
		peer = NULL;
		/* peer was removed, find the new first*/
		RTPSessionNextPeer(driver->rtp_session, &peer);
	}

	if (sock == driver->control_socket || sock == NULL) {
		if (driver->control_socket != NULL) {
			sock_release(driver->control_socket);
			driver->control_socket = NULL;
			pr_debug("control released\n");
		}
	}
	if (sock == driver->rtp_socket || sock == NULL) {
		if (driver->rtp_socket != NULL) {
			sock_release(driver->rtp_socket);
			driver->rtp_socket = NULL;
			pr_debug("rtp released\n");
		}
	}
	return 0;
}

void _applemidi_idle_timeout(unsigned long data)
{
	struct sockaddr_in *addr;
	int size;

	struct MIDIDriverAppleMIDI *driver = (struct MIDIDriverAppleMIDI *)data;
	mod_timer(&driver->timer, jiffies + msecs_to_jiffies(1500));

	pr_debug("====called timeout (%ld) dat: %lx ====\n", jiffies, data);

	if (spin_trylock(&(driver->lock))) {
		RTPSessionNextPeer(driver->rtp_session, &(driver->peer));
		if (driver->peer != NULL) {
			/* check if receiver feedback needs to be sent */
			if (driver->sync == 0) {
				/* no sync packets active. start new sync */
				RTPPeerGetAddress(driver->peer, &size, &addr);

				pr_debug("start sync with client: %pI4\n",
					 &addr->sin_addr.s_addr);
				_applemidi_start_sync(
				    driver, driver->rtp_socket->sk, size, addr);
				// no return
			}
		}
		spin_unlock(&(driver->lock));
	} else {
		pr_warn("could not so sync timeout because of midi event\n");
	}
	//
	// /* check for messages in dispatch (incoming) queue:
	//  *   if message needs to be dispatched (timestamp >= now+latency)
	//  *   call MIDIDriverAppleMIDIReceiveMessage
	//  * send receiver feedback
	//  * if the last synchronization happened a certain time ago,
	// synchronize again */
	//
	// return 0;
}

/**
 * @brief Create a MIDIDriverAppleMIDI instance.
 * Allocate space and initialize an MIDIDriverAppleMIDI instance.
 * @public @memberof MIDIDriverAppleMIDI
 * @return a pointer to the created driver structure on success.
 * @return a @c NULL pointer if the driver could not created.
 */
struct MIDIDriverAppleMIDI *MIDIDriverAppleMIDICreate(char *name,
						      unsigned short port)
{
	struct MIDIDriverAppleMIDI *driver;
	MIDITimestamp timestamp;

	driver = kmalloc(sizeof(struct MIDIDriverAppleMIDI), GFP_KERNEL);
	if (driver == NULL) {
		return NULL;
	}
	pr_debug("placed driver at %p\n", driver);

	spin_lock_init(&(driver->lock));

	pr_debug("allocated driver structure\n");
	MIDIDriverInit(&(driver->base), name, APPLEMIDI_CLOCK_RATE,
		       (void *)driver);

	driver->control_socket = NULL;
	driver->rtp_socket = NULL;
	driver->port = port;
	driver->accept = 0xff;
	driver->sync = 0;
	strncpy(&(driver->name[0]), name, sizeof(driver->name));
	driver->name[31] = 0;

	// driver->in_queue = MIDIMessageQueueCreate();
	// driver->out_queue = MIDIMessageQueueCreate();

	// driver->base.send = &_driver_send;
	// driver->base.destroy = &_driver_destroy;

	_applemidi_connect(driver);

	pr_debug("connected sockets\n");

	driver->peer = NULL;
	driver->rtp_session = RTPSessionCreate(driver->rtp_socket);
	driver->rtpmidi_session = RTPMIDISessionCreate(driver->rtp_session);

	MIDIClockGetNow(driver->base.clock, &timestamp);

	pr_debug("init timestamp with %lld\n", timestamp);

	driver->token = timestamp;

	memset(&(driver->command), 0, sizeof(driver->command));

	driver->command.peer = NULL;

	setup_timer(&driver->timer, _applemidi_idle_timeout,
		    (unsigned long)driver);

	mod_timer(&driver->timer, jiffies + msecs_to_jiffies(5000));

	return driver;
}

/**
 * @brief Destroy a MIDIDriverAppleMIDI instance.
 * Free all resources occupied by the driver.
 * @public @memberof MIDIDriverAppleMIDI
 * @param driver The driver.
 */
void MIDIDriverAppleMIDIDestroy(struct MIDIDriverAppleMIDI *driver)
{
	_applemidi_disconnect(driver, NULL);
	RTPMIDISessionRelease(driver->rtpmidi_session);
	RTPSessionRelease(driver->rtp_session);
	// MIDIMessageQueueRelease(driver->in_queue);
	// MIDIMessageQueueRelease(driver->out_queue);
	MIDIDriverRelease(&(driver->base));
	del_timer(&driver->timer);
	kfree(driver);
}

static int __init mod_init(void)
{
	pr_info("initializing applemidi\n");
	raspi = MIDIDriverAppleMIDICreate("kernel", port);
	if (raspi == NULL) {
		return 1;
	}
	return 0;
}

static void __exit mod_exit(void)
{
	if (raspi) {
		MIDIDriverAppleMIDIDestroy(raspi);
	}
	pr_info("leaving applemidi\n");
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
