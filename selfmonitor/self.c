#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../cr3spy_mod.h"

#define READONLY_DATA_SECTION_ASM_OP "\t.text"

struct mregion {
    unsigned int cr3;
    unsigned int lstart;
    unsigned int lend; 
};

/* int distant(void); */

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
    // now request self-monitoring at that mapped location

    struct mregion mydata;
    mydata.cr3 = cr3val;
    //mydata.lstart = (unsigned int)newbase;        
    mydata.lstart = (unsigned int)0;        
    //mydata.lend = (unsigned int)newbase + 4096;
    mydata.lend = (unsigned int)0;

    ret = ioctl(fd,IOCTL_TEST_MONITOR,&mydata);

    if(ret != 0) {
        fprintf(stderr,"failed to register my region\n");
    } else {
        printf("calling the function in the monitored region; should induce "
               "page fault\n");
    }

    //mmap a new page
    int fd2 = open("zeropage",O_RDWR);
    if(fd2 == -1) {
        perror("failed to open base file for mmap");
        exit(1);
    }
    void * newbase = mmap((void*)0,4096,PROT_READ,MAP_PRIVATE,fd2,0);
    if(newbase == MAP_FAILED) {
        perror("failed to mmap file");
        exit(1);
    }
    close(fd2);

    close(fd);    

    /* no, there's no function here. yes, it's going to crash. However,
        we should see some dmesg output in the xm console from taking
        a page fault */

    ret = ((int(*)(void))newbase)();

    //ret = distant();

    return 0;
}
/* this didn't work; apparently the page was already mapped in. 
asm("\t.align 4096");

int distant()
{
    return 31337;
}
*/
