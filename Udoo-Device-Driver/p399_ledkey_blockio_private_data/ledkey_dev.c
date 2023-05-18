#include <linux/init.h>
#include <linux/string.h>
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
#include <linux/slab.h>

#define CALL_DEV_NAME "ledkey_dev"
#define CALL_DEV_MAJOR 240
#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

DECLARE_WAIT_QUEUE_HEAD(WaitQueue_Read);

//static int sw_irq[8] = {0,};

typedef struct{
	int sw_irq[8];
	char sw_no;
} __attribute__ ((packed)) ISR_INFO;

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
irqreturn_t sw_isr(int irq, void* dev_id){
	int i;
	ISR_INFO* pSw_no = (ISR_INFO*)dev_id;
	pSw_no->sw_no = 0;
	for(i=0; i<ARRAY_SIZE(key);i++){
		if(irq == pSw_no->sw_irq[i]){
			pSw_no->sw_no = i+1;
			break;
		}
	}
	printk("IRQ : %d, SW_NO : %d\n",irq,pSw_no->sw_no);
	wake_up_interruptible(&WaitQueue_Read);
	return IRQ_HANDLED;
}
static int ledkey_request(struct file * filp)
{
	int ret=0;
	int i;
	ISR_INFO* pIsrInfo=NULL;
	pIsrInfo = (ISR_INFO*)filp->private_data;
	for (i = 0; i < ARRAY_SIZE(led); i++) {
		ret = gpio_request(led[i], "gpio led");
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", led[i], ret);
			break;
		} 
		gpio_direction_output(led[i],0);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++) {
		pIsrInfo->sw_irq[i] = gpio_to_irq(key[i]);
	}
	return ret;
}
static void ledkey_free(struct file * filp)
{
	int i;
	ISR_INFO* pIsr=NULL;
	pIsr = (ISR_INFO*)filp->private_data;
//	void* pDev = (void*)&pIsr->sw_no;
	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_free(led[i]);
	}
	for (i = 0; i < ARRAY_SIZE(key); i++){
		free_irq(pIsr->sw_irq[i],filp->private_data);
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
int irq_init(struct file * filp){
	ISR_INFO* pIsrIn=NULL;
	int i;
	int result=0;
	char* sw_name[8]={"key1","key2","key3","key4","key5","key6","key7","key8"};
	result = ledkey_request(filp);
	pIsrIn = (ISR_INFO*)filp->private_data;
	if(result < 0){
		return result;
	}
	for(i = 0; i < ARRAY_SIZE(sw_name);i++){
		result = request_irq(pIsrIn->sw_irq[i],sw_isr,IRQF_TRIGGER_RISING,sw_name[i],filp->private_data);
		if(result){
			printk("#### FAILED Request IRQ %d, error : %d\n",pIsrIn->sw_irq[i],result);
			break;
		}
	}
	return result;
}
static int ledkeydev_open(struct inode *inode,struct file *filp){
	int num = MINOR(inode->i_rdev);
	int num1 = MAJOR(inode->i_rdev);
	ISR_INFO* pIsrInfo;
	pIsrInfo = (ISR_INFO*)kmalloc(sizeof(ISR_INFO),GFP_KERNEL);

	printk("ledkeydev open -> minor : %d\n",num);
	printk("ledkeydev open -> major : %d\n",num1);
/*	pIsrInfo->sw_no = 0;
	for(i=0;i<8;i++){
		pIsrInfo->sw_irq[i] = 0;
	}*/
	memset(pIsrInfo,0x00,sizeof(ISR_INFO));
	filp->private_data = (void*)pIsrInfo;
	irq_init(filp);
	
	return 0;
}
static ssize_t ledkeydev_read(struct file *filp, char* buf, size_t count, loff_t *f_pos){
	int ret = 0;
	ISR_INFO* pSw_no = (ISR_INFO*)filp->private_data;
#if DEBUG
	printk("key read -> buf : %08X, count : %08X\n",(unsigned int)*buf,count);
#endif
	if(!(filp->f_flags & O_NONBLOCK)){			//Block mode Check
		if(pSw_no->sw_no == 0)
			interruptible_sleep_on(&WaitQueue_Read);
//			wait_event_interruptible(WaitQueue_Read,pSw_no->sw_no);
//			wait_event_interruptible_timeout(WaitQueue_Read,pSw_no->sw_no,100);
	}
	ret = copy_to_user(buf,&pSw_no->sw_no,count);
	pSw_no->sw_no = 0;
	if(ret<0)
		return -ENOMEM;
	return *buf;
}
static ssize_t ledkeydev_write(struct file *filp, const char* buf, size_t count, loff_t *f_pos){
	int ret = 0;
	char kbuf;
#if DEBUG
	printk("ledkeydev write -> buf : %08X, count : %08X\n",(unsigned int)*buf,count);
#endif
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
	led_write(0);
	ledkey_free(filp);
	if(filp->private_data)
		kfree(filp->private_data);
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
};

static int ledkeydev_init(void){
	int result=0;
	printk("call ledkeydev_init \n");
	result = register_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME, &ledkeydev_fops);
	if(result <0) return result;
	return result;
}
static void ledkeydev_exit(void){
	printk("call ledkeydev_exit \n");
	unregister_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME);
}

module_init(ledkeydev_init);
module_exit(ledkeydev_exit);
MODULE_LICENSE("Dual BSD/GPL");

