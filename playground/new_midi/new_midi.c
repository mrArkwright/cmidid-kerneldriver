#define DEBUG

#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
//#include <linux/ioctl.h>

#include <sound/core.h>
#include <sound/seq_kernel.h>



#define TEMPLATE "new_midi" /* Name of module */
#define LOGPREFIX "new_midi: "

static dev_t template_dev_number;
static struct cdev *driver_object;
static struct class *template_class;
static struct device *dev;

struct snd_card *card;
static int client = 0;

static int myint = 1;
module_param(myint, int, 0);

void config_note(struct snd_seq_event *note, unsigned char notevalue, unsigned char velocity) {
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

static void send_note(unsigned char notevalue, unsigned char velocity) {
	struct snd_seq_event note;
	int err;

	if (client > 0) {
		config_note(&note, notevalue, velocity);
		err = snd_seq_kernel_client_dispatch(client, &note, in_atomic(), 0);
		if (err < 0) {
			printk(KERN_ERR LOGPREFIX "Error dispatch client(%d) code:%d\n", client, err);
		}
	}
}

static long new_midi_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
	printk(KERN_INFO LOGPREFIX "ioctl called\n");
	
	send_note(0x3b, 127);
	
	return 0;
}

static struct file_operations new_midi_fops = {
    .unlocked_ioctl = new_midi_ioctl
};

static int init_midi(void) {
	int err;
	struct snd_seq_port_info *pinfo;
	
	err = snd_card_create(-1, NULL, THIS_MODULE, sizeof(struct snd_card), &card);
	if (err < 0) {
		printk(KERN_INFO LOGPREFIX "error creating card: %d\n", err);
		return err;
	}
	
	client = snd_seq_create_kernel_client(card, 0, "new_midi-client");
	if (client < 0) {
		printk(KERN_INFO LOGPREFIX "error creating client: %d\n", err);
		return err;
	}
	
	pinfo = kzalloc(sizeof(struct snd_seq_port_info), GFP_KERNEL);
	if (!pinfo) {
		err = -ENOMEM;
		return err;
	}
	pinfo->addr.client = client;
	//pinfo->capability |= SNDRV_SEQ_PORT_CAP_WRITE | SNDRV_SEQ_PORT_CAP_SYNC_WRITE | SNDRV_SEQ_PORT_CAP_SUBS_WRITE;
	pinfo->capability |= SNDRV_SEQ_PORT_CAP_READ | SNDRV_SEQ_PORT_CAP_SYNC_READ | SNDRV_SEQ_PORT_CAP_SUBS_READ;
	
	err = snd_seq_kernel_client_ctl(client, SNDRV_SEQ_IOCTL_CREATE_PORT, pinfo);
	if (err < 0) {
		printk(KERN_INFO LOGPREFIX "error creating port: %d\n", err);
		return err;
	}
	
	return 0;
}

void exit_midi(void) {
	snd_seq_delete_kernel_client(client);
	snd_card_free(card);
}

int __init new_midi_init(void) {
	printk(KERN_INFO LOGPREFIX "init\n");
	
	pr_info("myint: %d\n", myint);
	
	if (alloc_chrdev_region(&template_dev_number, 0, 1, TEMPLATE) < 0) return -EIO;
	
	driver_object = cdev_alloc();
	if (driver_object == NULL) goto free_device_number;
	driver_object->owner = THIS_MODULE;
	driver_object->ops = &new_midi_fops;
	
	if (cdev_add(driver_object, template_dev_number, 1)) goto free_cdev;
	
	template_class = class_create(THIS_MODULE, TEMPLATE);
	
	dev = device_create(template_class, NULL, template_dev_number, NULL, "%s", TEMPLATE);
	
	return init_midi();
	
free_cdev:
	kobject_put(&driver_object->kobj);
free_device_number:
	unregister_chrdev_region(template_dev_number, 1);
	return -EIO;
}

void __exit new_midi_exit(void) {
	exit_midi();
	
	printk(KERN_INFO LOGPREFIX "exit\n");
	
	/* Delete Sysfs entry and device file  */
	device_destroy(template_class, template_dev_number);
	class_destroy( template_class );
	/* Deletion of driver */
	cdev_del(driver_object);
	unregister_chrdev_region(template_dev_number, 1);
}

module_init(new_midi_init)
module_exit(new_midi_exit)

MODULE_LICENSE("GPL");