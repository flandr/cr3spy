#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm/page.h>
#include <string.h>

#include "../cr3spy_mod.h"

#define READONLY_DATA_SECTION_ASM_OP "\t.text"


/*

General idea of this test:

There is a function ("distant") aligned on a page boundary. What we're
going to do is map in another page containing some known data (text) that
would cause the program to misbehave if executed. We're going to install
a page pair where the text data is the "good" page and the distant function
is the "evil" page and then execute a call to the base address of the "good"
page. We'll also read some bytes out of that location. What we should see
is the text bytes being read but the distant function being executed.

*/

int distant(void); 

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
   
    /* map the region because we want to know the actual page it's
       mapped at in order to register the new technique. If we
       don't see a page fault with this technique, then it may
       mean that the page was faulted in when mmap was called
       so we'll need to so something trickier (like mmap with MAP_FIXED
       after /first/ registering the handler) */

    //mmap a new page
    int fd2 = open("evilpage",O_RDWR);
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

    /* the new method */
    struct new_monitor_info mi;
    mi.cr3 = cr3val;
    mi.good_page = PAGE_MASK & (unsigned long)newbase;
    mi.evil_page = PAGE_MASK & (unsigned long)distant;

    printf("Registering a monitored pair (0x%lx,0x%lx)\n",mi.good_page,
        mi.evil_page);

    ret = ioctl(fd,IOCTL_TEST_NEW_MONITOR,&mi); 
    if(ret != 0) {
        fprintf(stderr,"failed to register my region with the new method\n");
    }
 
    printf("Calling the \"function\" at the base address of the \"good page\";"
           "\nshould execute a function at the base address of the "
           "\"evil page\"\n");

    close(fd);    

    /* if everything is working correctly, distant will be executed. If
       everything is *not* working correctly, we'll execute some garbage
       and crash */
    ret = ((int(*)(void))(newbase))();

    printf("ret: %d\n",ret);

    printf("now going to read some characters from the \"good page\"...\n");

    char buf[16];
    memset(buf,0,16);

    strncpy(buf,(const char *)newbase,15);

    printf("read some bytes: %s\n",buf);

    

    return 0;
}
asm("\t.align 4096");

int distant()
{
    return 31337;
}
