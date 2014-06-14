#include <linux/module.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/sock.h>

#define SERVERPORT 5008
static struct socket *udpsocket = NULL;
//static struct socket *sendsocket = NULL;

u32 ssrc;

struct applemidi_session {
	__be32 addr;
	u16 port;
	bool valid;
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
	sess.addr = saddr;
	sess.port = port;

	int len;
	char buf[64];
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	struct sockaddr_in to;

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
	len = sock_sendmsg(udpsocket, &msg, 23);
	set_fs(oldfs);
	printk(KERN_ERR "sock_sendmsg returned: %d\n", len);
	if (len == 23)		// sent successfuly
	{
		sess.valid = true;
	}
	return 0;
}

static int session_end(void)
{
	sess.valid = false;
	return 0;
}

static void callback(struct sock *sk, int bytes)
{
	printk(KERN_ERR "--------------------------\n");
	printk(KERN_ERR "callback received %i bytes\n", bytes);

	int len = 0;
	int i = 0;

	struct sk_buff *skb;

	len = skb_queue_len(&sk->sk_receive_queue);
	while (len > 0) {
		skb = skb_dequeue(&sk->sk_receive_queue);

		struct iphdr *iph = (struct iphdr *)skb_network_header(skb);

		printk("saddr:%x\n", ntohl(iph->saddr));
		printk("daddr:%x\n", ntohl(iph->daddr));

		//printk("skb proto: %d\n" , skb->protocol);
		u16 *sp = (u16 *) skb->data;
		u16 sourceport = ntohs(*sp);
		u16 *l = (u16 *) (skb->data + 4);
		u16 length = ntohs(*l);
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
			u16 *sig = (u16 *) skb->data;
			printk("signature: %x\n", *sig);
			if (ntohs(*sig) == 0xffff)	//success, control package
			{
				printk("is control package\n");
				u16 *cmd = (u16 *) (skb->data + 2);
				u32 *token = (u32 *) (skb->data + 8);
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
	struct sockaddr_in server;
	//struct sockaddr_in sender;
	int servererror;

	if (sock_create(PF_INET, SOCK_DGRAM, IPPROTO_UDP, &udpsocket) < 0) {
		printk(KERN_ERR "server: Error creating udpsocket\n");
		return -EIO;
	}
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons((unsigned short)SERVERPORT);
	servererror =
	    udpsocket->ops->bind(udpsocket, (struct sockaddr *)&server,
				 sizeof(server));
	if (servererror) {
		sock_release(udpsocket);
		return -EIO;
	}

	udpsocket->sk->sk_data_ready = callback;

	/*if( sock_create( PF_INET,SOCK_DGRAM,IPPROTO_UDP,&sendsocket)<0 ){
	   printk( KERN_ERR "server: Error creating sendsocket\n" );
	   //should release udpsocket somewhere
	   return -EIO;
	   } */

	/*sender.sin_family = AF_INET;
	   sender.sin_addr.s_addr = in_aton("192.168.0.5");
	   sender.sin_port = htons( (unsigned short) SERVERPORT );
	   servererror = udpsocket->ops->bind(sendsocket, (struct sockaddr *) &sender, sizeof(sender ));
	   if( servererror ) {
	   printk("cound not bild send socket!\n");
	   return -EIO;
	   } */

	//get random ssrc:
	get_random_bytes(&ssrc, 4);
	printk("chose ssrc %x\n", ssrc);

	sess.valid = false;

	return 0;

}

static void __exit mod_exit(void)
{
	if (udpsocket)
		sock_release(udpsocket);
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
