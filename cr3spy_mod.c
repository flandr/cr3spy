#include <linux/module.h>
#include <linux/kernel.h>

/* character device stuff */
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/hypervisor.h>

#include "cr3spy_mod.h"

/* module functions */
int init_module(void);
void cleanup_module(void);
                                                                                
/* ioctl handler */
static int device_ioctl(struct inode *inode,
                        struct file *file,
                        unsigned int ioctl_num,
                        unsigned long ioctl_param);
 
static int MOD_USE_CNT = 0;
                                                                               
/* open and close handlers that manage module use count.
 * note that open will fail if module use count > 0 
 * (exclusive access only, given sequence of ioctls required
 * for information retrieval)
 */
static int device_open(struct inode *inode, struct file *file);
static int device_close(struct inode *inode, struct file *file);

/* initialize handlers we use */
static struct file_operations fops = {
    .ioctl = device_ioctl,
    .open = device_open,
    .release = device_close
};

int init_module(void)
{
    int ret;

    ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
    if(ret < 0)
    {
        printk("Registering %s device failed with %d\n",DEVICE_NAME,ret);
        return ret;
    }
    printk(KERN_INFO "CR3 Spy loaded.\n");

    return 0;
}

void cleanup_module(void)
{
    int ret;
    ret = unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    if(ret < 0)
        printk("Error unregistering device %s: %d\n", DEVICE_NAME, ret);
    printk(KERN_INFO "CR3 SPY unloaded.\n");
}

static int device_open(struct inode *inode, struct file *file)
{
    if(MOD_USE_CNT > 0)
        return -EBUSY;

    MOD_USE_CNT++;
 
    try_module_get(THIS_MODULE);
    return 0;
}

static int device_close(struct inode *inode, struct file *file)
{
    MOD_USE_CNT--;

    module_put(THIS_MODULE);
    return 0;
}

static int device_ioctl(struct inode *inode, 
                 struct file *file, 
                 unsigned int ioctl_num,
                 unsigned long ioctl_param)
{
    int ret = 0;
    unsigned long cr3 = read_cr3();

    struct monitor_info user_info;
    struct new_monitor_info new_user_info;

    switch(ioctl_num) {
        case IOCTL_GET_CR3:
            put_user(cr3,(long *)ioctl_param);
            break;
        case IOCTL_TEST_MONITOR:
            /* obtain user information for monitoring */
            ret = copy_from_user(&user_info,
                (struct monitor_info *)ioctl_param,
                sizeof(struct monitor_info));

            if(ret != 0) {
                printk("copy from user failed in monitor reg!\n");
                return -1;
            }

            /* execute a hypercall to Xen */

            ret = HYPERVISOR_monitor_region(user_info.cr3,
                        user_info.lstart,user_info.lend);
        
            if(ret != 0) {
                printk("failed to register monitor request\n"); 
            } else {
            printk("monitor request from 0x%lx-0x%lx in 0x%lx registered: %d\n",
                user_info.lstart,user_info.lend,user_info.cr3,ret);
            }
            break;
        case IOCTL_TEST_NEW_MONITOR:
            ret = copy_from_user(&new_user_info,
                (struct new_monitor_info *)ioctl_param,
                sizeof(struct new_monitor_info));

            if(ret != 0) {
                printk("copy from user failed in monitor reg!\n");
                return -1;
            }

            ret = HYPERVISOR_associate_page_pair(
                    new_user_info.cr3,
                    new_user_info.evil_page,
                    new_user_info.good_page);

            if(ret != 0) {
                printk("failed to register monitor request\n"); 
            } else {
                printk("monitor request for 0x%lx (evil: 0x%lx) in 0x%lx registered: %d\n",
                new_user_info.good_page,
                new_user_info.evil_page,
                new_user_info.cr3,ret);
            }
            break;
        

        default:
            return -1;
    }
    return 0;
}
