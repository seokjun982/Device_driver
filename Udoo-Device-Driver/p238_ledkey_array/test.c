#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#define CALL_DEV_NAME "ledkey_dev"
#define CALL_DEV_MAJOR 240
#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

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
	}
	for (i = 0; i < ARRAY_SIZE(key); i++) {
		ret = gpio_request(key[i], "gpio key");
		if(ret<0){
			printk("#### FAILED Request gpio %d. error : %d \n", key[i], ret);
			break;
		} 
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
		gpio_free(key[i]);
	}
}
static void led_write(unsigned char* data)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(led); i++){
		gpio_direction_output(led[i], (data[i] >> i ) & 0x01);
//		gpio_set_value(led[i], (data >> i ) & 0x01);
	}
#if DEBUG
	printk("#### %s, data = %d\n", __FUNCTION__, *data);
#endif
}
static void key_read(unsigned char* key_data)
{
	int i;
	unsigned long data[8]={0,};
	unsigned long temp[8];
	for(i=0;i<8;i++)
	{
  		gpio_direction_input(key[i]); //error led all turn off
		temp[i] = gpio_get_value(key[i]); /*<< i;*/
		data[i] |= temp[i];
	}
/*	
	for(i=3;i>=0;i--)
	{
		gpio_direction_input(led[i]); //error led all turn off
		temp = gpio_get_value(led[i]);
		data |= temp;
		if(i==0)
			break;
		data <<= 1;  //data <<= 1;
	}
*/
//#if DEBUG
//	printk("#### %s, data = %ld\n", __FUNCTION__,data);
//#endif
	for(i=0;i<8;i++){
		key_data[i] = data[i];
	}
	return;
}
static int leddev_open(struct inode *inode,struct file *filp){
	int num = MINOR(inode->i_rdev);
	printk("leddev open -> minor : %d\n",num);
	num = MAJOR(inode->i_rdev);
	printk("leddev open -> major : %d\n",num);
	return 0;
}
static loff_t leddev_llseek(struct file *filp, loff_t off, int whence){
	printk("leddev llseek -> off : %08X, whence : %08X\n",(unsigned int)off,whence);
	return 0x23;
}
static ssize_t leddev_read(struct file *filp, char* buf, size_t count, loff_t *f_pos){
	unsigned char kbuf[8]={0,};
	int ret = 0;
//	int i = 0;
//	for(i = 0; i < 8; i++){
//		if(buf[i]!=0)
//			printk("key read -> buf[%d] : %08X, count : %08X\n",i,(unsigned int)buf[i],count);
//	}
	key_read(kbuf);
//	put_user(kbuf,buf);
	if((ret = copy_to_user(buf,kbuf,count))<0)
		return -ENOMEM;
	return *buf;
}
static ssize_t leddev_write(struct file *filp, const char* buf, size_t count, loff_t *f_pos){
	unsigned char kbuf[4]={0,};
	int ret = 0;
	printk("leddev write -> buf : %08X, count : %08X\n",(unsigned int)*buf,count);
//	get_user(kbuf,buf);
	if((ret = copy_from_user(kbuf,buf,count))<0)
		return -ENOMEM;
	led_write(kbuf);
	return *kbuf;
}
static long leddev_ioctl(struct file *filp,unsigned int cmd,unsigned long arg){
	printk("leddev ioctl -> cmd : %08X, arg : %08X\n",cmd,(unsigned int)arg);
	return 0x53;
}
static int leddev_release(struct inode *inode,struct file *filp){
	printk("leddev release\n");
	return 0;
}
struct file_operations leddev_fops =
{
	.owner = THIS_MODULE,
	.llseek = leddev_llseek,
	.read = leddev_read,
	.write = leddev_write,
	.unlocked_ioctl = leddev_ioctl,
	.open = leddev_open,
	.release = leddev_release,	//close
};
static int leddev_init(void){
	int result;
	printk("call leddev_init \n");
	result = register_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME, &leddev_fops);
	if(result <0) return result;
	ledkey_free();
	result = ledkey_request();
	if(result < 0){
		return result;
	}
	return 0;
}
static void leddev_exit(void){
	printk("call leddev_exit \n");
	led_write(0);
	ledkey_free();
	unregister_chrdev(CALL_DEV_MAJOR,CALL_DEV_NAME);
}

module_init(leddev_init);
module_exit(leddev_exit);
MODULE_LICENSE("Dual BSD/GPL");

