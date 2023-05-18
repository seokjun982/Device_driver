#include <stdio.h>
//#include <asm/uaccess.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_FILENAME "/dev/ledkey_dev"
void print_key(unsigned char);
void print_led(unsigned char);

int main(int argc,char* argv[]){
	int dev;
	char buff[128];
	int ret;
	char oldbuf = 0;
	if(argc < 2)
	{
		printf("Usage : %s led_val\n",argv[0]);
		return 1;
	}
	char argint = atoi(argv[1]);

//	printf("1) device file open\n");
	dev = open(DEVICE_FILENAME, O_RDWR | O_NDELAY);
	if(dev < 0){
			perror("open()");
			return 2;
	}
	ret = write(dev,&argint,0x01);
	if(ret < 0){
		perror("write()");
		return 2;
	}
	print_led(argint);
	argint = 0;
	do{
 		read(dev,&argint,sizeof(argint));
		if(argint != 0 && argint != oldbuf){
			print_key(argint);
			write(dev,&argint,sizeof(argint));
			print_led(argint);
			oldbuf = argint;
			if(argint == 8)
				break;
		}
	}while(1);

	ret = ioctl(dev,0x51,0x52);
	ret = close(dev);
	return 0;
}
void print_led(unsigned char led){
		int i=0;
		puts("1:2:3:4");
		for(i=0;i<4;i++)
	    {
	       if(led & (0x1<<i))
	           putchar('O');
	       else
	           putchar('X');
		   if(i != 3)
		       putchar(':');
		   else
		       putchar('\n');
		}
		return;
}

void print_key(unsigned char key){
		int i = 0;
		puts("1:2:3:4:5:6:7:8");
		for(i=0;i<=7;i++)
	    {
	       if(key==i+1)
	           putchar('O');
	       else
	           putchar('X');
		   if(i < 7)
		       putchar(':');
		   else
		       putchar('\n');
		}
		return;
}
