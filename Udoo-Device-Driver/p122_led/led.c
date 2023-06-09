#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>

#define DEBUG 1
#define IMX_GPIO_NR(bank, nr)       (((bank) - 1) * 32 + (nr))

static unsigned long  ledvalue = 15;
static char* twostring = NULL;

module_param(ledvalue, ulong, 0);
module_param(twostring, charp, 0);

int led[] = {
	IMX_GPIO_NR(1, 16),   //16
	IMX_GPIO_NR(1, 17),	  //17
	IMX_GPIO_NR(1, 18),   //18
	IMX_GPIO_NR(1, 19),   //19
};

static void led_write(unsigned long data)
{
	int i;
	for(i = 0; i < ARRAY_SIZE(led); i++){
		gpio_direction_output(led[i], (data >> i ) & 0x01);
//		gpio_set_value(led[i], (data >> i ) & 0x01);
	}
#if DEBUG
	printk("#### %s, data = %ld\n", __FUNCTION__, data);
#endif
}

static int led_request(void)
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
	return ret;
}
static void led_free(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(led); i++){
		gpio_free(led[i]);
	}
}
static int led_init(void){
	int ret = 0;
	printk(KERN_DEBUG "Hello, world [ledvalue = %lu,twostring = %s]\n",ledvalue,twostring);
	ret = led_request();
	if(ret<0){
		return ret;			/*Device or resource busy*/
//		return -EBUSY;		/*Device or resource busy*/
	}
	led_write(ledvalue);
//	led_free();
	return 0;
}

static void led_exit(void){
	printk(KERN_DEBUG "Goodbye, world");
//	led_request();
	led_write(0);
	led_free();
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("Dual BSD/GPL");
