#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(int argc,char* argv[]){

	int ret;

	ret = mknod("/dev/testdev1", S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH|S_IFCHR, (240 << 8)|1);
	if(ret < 0){
		perror("mknod()");
		return 1;
	}
	ret = open("/dev/testdev1",O_RDWR);	
	if(ret < 0){
		perror("open()");
//		return -EPERM;
		return 2;
	}
	return 0;
}
