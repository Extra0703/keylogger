#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
//#include <linux/sched/signal.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define IRQ_NO		1
#define MEM_SIZE	1
//#define REG_KB_MON	_IOW('a', 'a', int32_t *)
//#define SIGNUM		44

MODULE_LICENSE("GPL");

dev_t dev = 0;
static struct class *dev_class;
static struct cdev chr_cdev;
static uint8_t *kernel_buffer;
static DECLARE_WAIT_QUEUE_HEAD(wq);
static bool can_read = false;
static struct kobject *kobj_ref;

//static struct task_struct *task = NULL;

static int 	__init keyboard_simulator_init(void);
static void	__exit keyboard_simulator_exit(void);

static int		chr_open(struct inode *, struct file *);
static int		chr_release(struct inode *, struct file *);
static ssize_t		chr_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t		chr_write(struct file *, const char __user *, size_t, loff_t *);
//static long		chr_ioctl(struct file *, unsigned int, unsigned long);
static unsigned int 	chr_poll(struct file *, struct poll_table_struct *);

static ssize_t		sysfs_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t		sysfs_store(struct kobject *, struct kobj_attribute *, const char *, size_t);
struct kobj_attribute	chr_attr = __ATTR(kernel_buffer, 0660, sysfs_show, sysfs_store);

static irqreturn_t irq_handler(int, void *);

static irqreturn_t irq_handler(int irq, void *dev_id){
	/*struct kernel_siginfo info;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIGNUM;
        info.si_code = SI_QUEUE;
	info.si_int = 1;

	if(task != NULL){
		printk(KERN_INFO "Keyboard Simulator: sending signal to monitor\n");
		if(send_sig_info(SIGNUM, &info, task) < 0){
			printk(KERN_INFO "Keyboard Simulator: sending signal failed\n");
		}
	}*/

//	char scancode;
//	scancode = inb(0x60);
//	printk(KERN_INFO "%c", scancode & 0x7f);

	return IRQ_HANDLED;
} 

static struct file_operations fops = {
	.owner		= THIS_MODULE,
	.read		= chr_read,
	.write		= chr_write,
	.open		= chr_open,
	.release	= chr_release,
	//.unlocked_ioctl	= chr_ioctl,
	.poll		= chr_poll,
};

static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	return sprintf(buf, "%c", kernel_buffer);
//	return 0;
}

static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count){
	strcpy(kernel_buffer, buf);

//	can_read = true;
	
//	wake_up(&wq);

	return count;
}

static int chr_open(struct inode *inode, struct file *filp){
	printk(KERN_INFO "Keyboard Simulator: open method\n");
	return 0;
}


static int chr_release(struct inode *inode, struct file *filp){
//	struct task_struct *ref_task = get_current();
	printk(KERN_INFO "Keyboard Simulator: release method\n");

/*	if(ref_task == task){
		task = NULL;
	}
*/
	return 0;
}


static ssize_t chr_read(struct file *filp, char __user *buf, size_t len, loff_t *off){
	printk(KERN_INFO "Keyboard Simulator: read method\n");

	if(copy_to_user(buf, kernel_buffer, MEM_SIZE)){
		printk(KERN_INFO "Keyboard Simulator: data read error!\n");
	}

	//can_read = false;
//	strcpy(buf, kernel_buffer);

	return MEM_SIZE;
}


static ssize_t chr_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	printk(KERN_INFO "Keyboard Simulator: write method\n");
	
	if(copy_from_user(kernel_buffer, buf, len)){
		printk(KERN_INFO "Keyboard Simulator: data write error!\n");
	}

	can_read = true;

	wake_up(&wq);

//	strcpy(kernel_buffer, buf);

	return len;
}

/*static long chr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	switch(cmd){
		case REG_KB_MON:
			printk(KERN_INFO "Keyboard Simulator: register keyboard monitor process\n");
			task = get_current();
			break;
		default:
			printk(KERN_INFO "Keyboard Simulator: wrong command\n");
			break;
	}
	return 0;
}*/

static unsigned int chr_poll(struct file *filp, struct poll_table_struct *wait){
	printk(KERN_INFO "Keyboard Simulator: poll method, %s\n", can_read ? "True" : "False");
	__poll_t mask = 0;

	poll_wait(filp, &wq, wait);

	if(can_read){
		can_read = false;
		mask |= (POLLIN | POLLRDNORM);
		printk(KERN_INFO "Keyboard Simulator: mask assigned\n");
	}

	return mask;
}

static int __init keyboard_simulator_init(void){
	if(alloc_chrdev_region(&dev, 0, 1, "keyboard_simulator_Dev") < 0){
		printk(KERN_INFO "Keyboard Simulator: Cannot allocate major number\n");
		return -1;
	}
	
	printk(KERN_INFO "Keyboard Simulator: Major: %d, Minor: %d\n", MAJOR(dev), MINOR(dev));

	cdev_init(&chr_cdev, &fops);

	if(cdev_add(&chr_cdev, dev, 1) < 0){
		printk(KERN_INFO "Keyboard Simulator: Cannot add the device to the system\n");
		goto r_class;
	}

	if(IS_ERR(dev_class = class_create(THIS_MODULE, "keyboard_simulator_class"))){
		printk(KERN_INFO "Keyboard Simulator: Cannot create the class structure\n");
		goto r_class;
	}
	
	if(IS_ERR(device_create(dev_class, NULL, dev, NULL, "keyboard_simulator"))){
		printk(KERN_INFO "Keyboard Simulator: Cannot create the device\n");
		goto r_device;
	}

	if((kernel_buffer = kmalloc(MEM_SIZE, GFP_KERNEL)) == 0){
		printk(KERN_INFO "Keyboard Simulator: Cannot allocate memory in kernel\n");
		goto r_device;
	}

	kobj_ref = kobject_create_and_add("keyboard_simulator", kernel_kobj);

	if(sysfs_create_file(kobj_ref, &chr_attr.attr)){
		printk(KERN_INFO "Keyboard Simulator: Cannot create sysfs file...\n");
		goto r_sysfs;
	}

	if(request_irq(IRQ_NO, irq_handler, IRQF_SHARED, "keyboard_simulator", (void *)(irq_handler))){
		printk(KERN_INFO "keyboard simulator: Cannot register IRQ\n");
		goto irq;
	}


	printk(KERN_INFO "Keyboard Simulator: init sucesses\n");
	return 0;

irq:
	free_irq(IRQ_NO, (void *)(irq_handler));

r_sysfs:
	kobject_put(kobj_ref);
	sysfs_remove_file(kernel_kobj, &chr_attr.attr);

r_device:
	class_destroy(dev_class);

r_class:
	unregister_chrdev_region(dev, 1);
	return -1;
}

static void __exit keyboard_simulator_exit(void){
	free_irq(IRQ_NO, (void *)(irq_handler));
	kobject_put(kobj_ref);
	sysfs_remove_file(kernel_kobj, &chr_attr.attr);
	kfree(kernel_buffer);
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&chr_cdev);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "Keyboard Simulator: exit sucesses\n");
}

module_init(keyboard_simulator_init);
module_exit(keyboard_simulator_exit);
