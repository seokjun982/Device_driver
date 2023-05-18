#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_FILENAME "/dev/leddev"

int main(int argc,char* argv[]){
	int dev;
	char buff[128];
	int ret;


//	printf("1) device file open\n");
	dev = open(DEVICE_FILENAME, O_RDWR | O_NDELAY);

//	printf("dev : %d\n",dev);

	if(argc < 2)
	{
		printf("Usage : %s led_val\n",argv[0]);
		return 1;
	}
	char argint = atoi(argv[1]);
//	printf("argv[1] : %d\n",argint);
//	while(1){
//	
//	}

	if(dev >=0){
//		printf("2) seek function call dev : %d \n",dev);
		ret = lseek(dev,0x20,SEEK_SET);
//		printf("ret = %08X\n",ret);
//		printf("3) read function call\n");
//		printf("ret = %08X\n",ret);
//		printf("4) write function call\n");
		ret = write(dev,&argint,0x41);
		if(ret < 0){
			perror("write()");
			return 2;
		}
		ret = read(dev,&argint,0x31);
		puts("1:2:3:4");
		int i=0;
		for(i=0;i<4;i++)
	    {
	       if(ret & (0x1<<i))
	           putchar('O');
	       else
	           putchar('X');
		   if(i != 3)
		       putchar(':');
		   else
		       putchar('\n');
		}
//		printf("ret = %08X\n",ret);
//		printf("5) ioctl function call\n");
		ret = ioctl(dev,0x51,0x52);
//		printf("ret = %08X\n",ret);
//		printf("6) device file close\n");
		ret = close(dev);
//		printf("ret = %08x\n",ret);i
	}
	else{
		perror("open");
	}
	return 0;
}
