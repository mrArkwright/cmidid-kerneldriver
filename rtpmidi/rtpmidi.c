#include <linux/module.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/ktime.h>
#include <net/sock.h>

#define SERVERPORT 5008
static struct socket *controlsocket = NULL;
static struct socket *datasocket = NULL;

u32 ssrc;

struct applemidi_session {
	__be32 addr;
	u16 port;

	u64 latency, difference;

	bool control;
	bool data;
};

struct applemidi_session sess;

struct applemidi_packet {
	u16 signature;
	u16 command;
	u32 version;
	u32 token;
	u32 ssrc;
	char name[40];
};

static int session_init(u32 saddr, u16 port, u32 token)
{
	//reject if already valid

	int len;
	char buf[64];
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	struct sockaddr_in to;

	if (sess.control == false)	// control init
	{
		sess.addr = saddr;
		sess.port = port;
	}

	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(saddr);	/* destination address */
	to.sin_port = htons(port);
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &to;
	msg.msg_namelen = sizeof(to);
	*(u16 *) (buf) = htons(0xffff);
	*(u16 *) (buf + 2) = htons(0x4f4b);
	*(u32 *) (buf + 4) = htons(2);
	*(u32 *) (buf + 8) = htonl(token);
	*(u32 *) (buf + 12) = htonl(ssrc);
	memcpy(buf + 16, "kernel", 7);
	iov.iov_base = buf;
	iov.iov_len = 16 + 7;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	// msg.msg_flags    = MSG_NOSIGNAL;
	printk(KERN_ERR " vor send\n");
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	if (port % 2 == 0) {
		len = sock_sendmsg(controlsocket, &msg, 23);
	} else {
		len = sock_sendmsg(datasocket, &msg, 23);
	}
	set_fs(oldfs);
	printk(KERN_ERR "sock_sendmsg returned: %d\n", len);
	if (len == 23)		// sent successfuly
	{
		if (sess.control == false) {
			sess.control = true;
		} else {
			sess.data = true;
		}
	}
	return 0;
}

static int session_end(void)
{
	sess.data = false;
	sess.control = false;
	return 0;
}

static void session_sync(u32 saddr, u16 port, char *data)
{
	u8 *count;
	ktime_t nsnow;
	u64 now;

	int tmp;

	count = (u8 *) data;
	nsnow = ktime_get();

	//10khz sampling
	tmp = nsnow.tv64 >> 16 & 0xffffffff;
	now = tmp / 1.52587;

	printk("time is %ld\n", now);
	if (*count == 0) {
		printk("we have to respond to sync\n");
	}
	return;
}

static void callback(struct sock *sk, int bytes)
{
	int len = 0;
	int i = 0;

	struct sk_buff *skb;

	struct iphdr *iph;

	u16 *sp, *l, sourceport, length;
	u16 *sig, *cmd;
	u32 *token;

	printk(KERN_ERR "--------------------------\n");
	printk(KERN_ERR "callback received %i bytes\n", bytes);

	len = skb_queue_len(&sk->sk_receive_queue);
	while (len > 0) {
		skb = skb_dequeue(&sk->sk_receive_queue);

		iph = (struct iphdr *)skb_network_header(skb);

		printk("saddr:%x\n", ntohl(iph->saddr));
		printk("daddr:%x\n", ntohl(iph->daddr));

		//printk("skb proto: %d\n" , skb->protocol);
		sp = (u16 *) skb->data;
		sourceport = ntohs(*sp);
		l = (u16 *) (skb->data + 4);
		length = ntohs(*l);
		length -= 8;
		skb->data += 8;	// cut udp header
		printk(KERN_ERR "ports: source %hu len %hu\n", sourceport,
		       length);
		printk("packet: ");
		for (i; i < length; i++) {
			u8 *p = (u8 *) skb->data + i;
			printk("%x ", *p);
		}
		printk("\n");
		//check for valid appleMIDI packet
		if (length >= 16 && length < 56) {
			printk("correct length\n");
			sig = (u16 *) skb->data;
			printk("signature: %x\n", *sig);
			if (ntohs(*sig) == 0xffff)	//success, control package
			{
				printk("is control package\n");
				cmd = (u16 *) (skb->data + 2);
				token = (u32 *) (skb->data + 8);
				printk("cmd: %x\n", ntohs(*cmd));
				switch (ntohs(*cmd)) {
				case 0x494e:	//IN invitation
					printk("is invitation package\n");
					session_init(ntohl(iph->saddr),
						     sourceport, ntohl(*token));
					break;
				case 0x4259:	//BY end
					printk("is end package\n");
					session_end();
					break;
				case 0x434b:	//CK clock synch
					printk("is synchronisation packet\n");
					session_sync(ntohl(iph->saddr),
						     sourceport, skb->data + 8);
					break;
				}
			}
		}

		len--;
		kfree_skb(skb);
	}
	return;
}

static int __init mod_init(void)
{
	struct sockaddr_in controlserver;
	struct sockaddr_in dataserver;
	int servererror;

	if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &controlsocket) < 0) {
		printk(KERN_ERR "server: Error creating controlsocket\n");
		return -EIO;
	}
	controlserver.sin_family = AF_INET;
	controlserver.sin_addr.s_addr = INADDR_ANY;
	controlserver.sin_port = htons((unsigned short)SERVERPORT);
	servererror =
	    controlsocket->ops->bind(controlsocket,
				     (struct sockaddr *)&controlserver,
				     sizeof(controlserver));
	if (servererror) {
		printk(KERN_ERR "could not bind control socket");
		goto control_fail;
	}

	controlsocket->sk->sk_data_ready = callback;

	if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &datasocket) < 0) {
		printk(KERN_ERR "server: Error creating datasocket\n");
		goto control_fail;
	}

	dataserver.sin_family = AF_INET;
	dataserver.sin_addr.s_addr = INADDR_ANY;
	dataserver.sin_port = htons((unsigned short)(SERVERPORT + 1));
	servererror =
	    datasocket->ops->bind(datasocket, (struct sockaddr *)&dataserver,
				  sizeof(dataserver));
	if (servererror) {
		printk("cound not bind data socket!\n");
		goto data_fail;
	}

	datasocket->sk->sk_data_ready = callback;

	//get random ssrc:
	get_random_bytes(&ssrc, 4);
	printk("chose ssrc %x\n", ssrc);

	sess.control = false;
	sess.data = false;
	sess.latency = 0;
	sess.difference = 0;

	return 0;

 data_fail:
	sock_release(datasocket);

 control_fail:
	sock_release(controlsocket);

	return -EIO;

}

static void __exit mod_exit(void)
{
	if (controlsocket)
		sock_release(controlsocket);
	if (datasocket)
		sock_release(datasocket);
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
