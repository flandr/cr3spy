#ifndef KINST_MOD_H
#define KINST_MOD_H

#define DEVICE_NAME "cr3spy"
#define MAJOR_NUM 200       /* deliciously ARBITRARY */

#define read_cr3() ({ \
    unsigned long __dummy; \
    __asm__ ( \
        "movl %%cr3,%0\n\t" \
        :"=r" (__dummy)); \
    __dummy; \
})

struct monitor_info {
    unsigned long cr3;
    unsigned long lstart;
    unsigned long lend;
};

struct new_monitor_info {
    unsigned long cr3;
    unsigned long good_page;
    unsigned long evil_page;
};

/* set up our ioctls */

 /* ioctl to retrieve the current CR3 value:
  * the PTBR of the current process */
#define IOCTL_GET_CR3 _IOR(MAJOR_NUM, 0, long)

#endif
