#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>

#include "../cr3spy_mod.h"


int main(int argc, char**argv) {
    int fd;

    unsigned long cr3val;

    fd = open("/dev/"DEVICE_NAME,O_RDONLY);
    if(fd<0) {
        fprintf(stderr,"Can't open device /dev/%s\n",DEVICE_NAME);
        return -1;
    }

    int ret = ioctl(fd, IOCTL_GET_CR3,&cr3val);
    if(ret < 0) {
        fprintf(stderr,"ioctl_get_cr3 failed: %d\n",ret);
    } else {
        printf("current process cr3: 0x%lx\n",cr3val);
    }

    close(fd);
    return 0;
}

