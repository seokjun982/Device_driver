#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define CALL_DEV_NAME "ledkey_dev"
#define CALL_DEV_MAJOR 240
#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

DECLARE_WAIT_QUEUE_HEAD(WaitQueue_Read);

static int sw_irq[8] = {0,};
static char sw_no = 0;

int led[4] = {
		IMX_GPIO_NR(1, 16),   //16
		IMX_GPIO_NR(1, 17),	  //17
		IMX_GPIO_NR(1, 18),   //18
		IMX_GPIO_NR(1, 19),   //19
};
int key[8] = {
		IMX_GPIO_NR(1, 20),   //SW1 20
		IMX_GPIO_NR(1, 21),	  //SW2 21
		IMX_GPIO_NR(4, 8),   //SW3 104
		IMX_GPIO_NR(4, 9),   //SW4 105
		IMX_GPIO_NR(4, 5),   //SW5 101
		IMX_GPIO_NR(7, 13),	  //SW6 205
		IMX_GPIO_NR(1, 7),   //SW7 7
		IMX_GPIO_NR(1, 8),   //SW8 8
};
irqreturn_t sw_isr(int irq, void* unuse){
	int i;
	for(i=0; i<ARRAY_SIZE(key);i++){
		if(irq == sw_irq[i]){
			sw_no = i+1;
			break;
		}
	}
	printk("IRQ : %d, SW_NO : %d\n",irq,sw_no);
	wake_up_interruptible(&WaitQueue_Read);
	return IRQ_HANDLED;
}
static int ledkey_request(void)
{
	int ret=0;
	int i;
	for (i = 0; i < ARRAY_SIZE(led); i++) {
		ret = gpio_request(led[i], "gpio led");
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", led[i], ret);
			break;
		} 
		gpio_direction_output(led[i],0);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++) {
		sw_irq[i] = gpio_to_irq(key[i]);
	}
	return ret;
}
static void ledkey_free(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_free(led[i]);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++){
		free_irq(sw_irq[i],NULL);
	}
}
static void led_write(unsigned char data)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(led); i++){
		gpio_set_value(led[i], (data >> i ) & 0x01);
	}
#if DEBUG
	printk("#### %s, data = %d\n", __FUNCTION__, data);
#endif
}
void key_read(unsigned char* key_data)
{
	int i;
	char data=0;
	for(i=0;i<ARRAY_SIZE(key);i++)
	{
  		if(gpio_get_value(key[i])){ 
			data = i+1;
			break;
		}
	}
#if DEBUG
	printk("#### %s, data = %ld\n", __FUNCTION__,(unsigned long)data);
#endif
	*key_data = data;
	return;
}
static int ledkeydev_open(struct inode *inode,struct file *filp){
	int num = MINOR(inode->i_rdev);
	int result = 0;
	int i;
	char* sw_name[8]={"key1","key2","key3","key4","key5","key6","key7","key8"};

	printk("ledkeydev open -> minor : %d\n",num);
	num = MAJOR(inode->i_rdev);
	printk("ledkeydev open -> major : %d\n",num);
	for(i = 0; i < ARRAY_SIZE(sw_irq);i++){
		result = request_irq(sw_irq[i],sw_isr,IRQF_TRIGGER_RISING,sw_name[i],NULL);
		if(result){
			printk("#### FAILED Request IRQ %d, error : %d\n",sw_irq[i],result);
			break;
		}
	}
	return 0;
}
static unsigned int ledkeydev_poll(struct file * filp, poll_table * wait){
	unsigned int mask = 0;
	printk("_key : %ld/n",(wait->_key & POLLIN));
	if(wait->_key & POLLIN){
		poll_wait(filp,&WaitQueue_Read,wait);
	}
	if(sw_no > 0){
		mask = POLLIN;
	}
	return mask;
}
static ssize_t ledkeydev_read(struct file *filp, char* buf, size_t count, loff_t *f_pos){
	int ret = 0;
#if DEBUG
	printk("key read -> buf : %08X, count : %08X\n",(unsigned int)*buf,count);
#endif
	if(!(filp->f_flags & O_NONBLOCK)){
		if(sw_no == 0)
			interruptible_sleep_on(&WaitQueue_Read);
//		wait_event_interruptible(WaitQueue_Read,sw_no);
//		wait_event_interruptible_timeout(WaitQueue_Read,sw_no,100);		//100 : 100 * 100 = 1sec
	}
	ret = copy_to_user(buf,&sw_no,count);
	sw_no = 0;
	if(ret<0)
		return -ENOMEM;
	return *buf;
}
static ssize_t ledkeydev_write(struct file *filp, const char* buf, size_t count, loff_t *f_pos){
	unsigned char kbuf;
	int ret = 0;
#if DEBUG
	printk("ledkeydev write -> buf : %08X, count : %08X\n",(unsigned int)*buf,count);
#endif
//	get_user(kbuf,buf);
	if((ret = copy_from_user(&kbuf,buf,count))<0)
		return -ENOMEM;
	led_write(kbuf);
	return kbuf;
}
static long ledkeydev_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
#if DEBUG
	printk("ledkeydev ioctl -> cmd : %08X, arg : %08X\n",cmd,(unsigned int)arg);
#endif
	return 0x53;
}
static int ledkeydev_release(struct inode *inode,struct file *filp){
	printk("ledkeydev release\n");
	return 0;
}
struct file_operations ledkeydev_fops =
{
	.owner = THIS_MODULE,
	.read = ledkeydev_read,
	.write = ledkeydev_write,
	.unlocked_ioctl = ledkeydev_ioctl,
	.open = ledkeydev_open,
	.release = ledkeydev_release,	//close
	.poll= ledkeydev_poll,
};
static int ledkeydev_init(void){
	int result=0;
//	int i;
//	char* sw_name[8]={"key1","key2","key3","key4","key5","key6","key7","key8"};
	printk("call ledkeydev_init \n");
	result = register_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME, &ledkeydev_fops);
	if(result <0) return result;
	result = ledkey_request();
	if(result < 0){
		return result;
	}
/*	for(i = 0; i < ARRAY_SIZE(sw_irq);i++){
		result = request_irq(sw_irq[i],sw_isr,IRQF_TRIGGER_RISING,sw_name[i],NULL);
		if(result){
			printk("#### FAILED Request IRQ %d, error : %d\n",sw_irq[i],result);
			break;
		}
	}*/
	return result;
}
static void ledkeydev_exit(void){
	printk("call ledkeydev_exit \n");
	led_write(0);
	ledkey_free();
	unregister_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME);
}

module_init(ledkeydev_init);
module_exit(ledkeydev_exit);
MODULE_LICENSE("Dual BSD/GPL");

