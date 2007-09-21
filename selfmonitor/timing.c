#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm/page.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>

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

/* General idea:
 
   Map in a large number of pages that will be the "execute" pages and
   fill them up with nop instructions.

   Map in a large number of pages that will be the "read" pages and fill
   them up with "unimpl" instructions.

   Create the mappings where the execute pages are "evil" and the
   "read" pages are good.

   Now access every byte in two patterns: first by doing all the reads
   N times, then all of the executes N times, and then by doing the same
   interleved.

   We should measure the following:
        time, user-side
        fault-rate, xen-side
        micro-timing, xen-side
*/

//#define PAGE_SIZE 4096
static unsigned int PAGE_CNT = 100;
static int EXP = -1;
static bool NO_MAP = false;

#define HLT 0xF4
#define NOP 0x90

asm("\t.align 4096\n");

typedef struct __page {
    char p[PAGE_SIZE];
} page_s;

page_s *execute;
page_s *rdwr;

int helper_fd;

static void tramp_dummy(void) __attribute__ ((used));
static void tramp_dummy(void)
{
    __asm__ __volatile__(".text\n"
        "__tramp_patch_start:\n"
        "push %ebx;"
        "movl $exec_tramp_return_loc,%ebx;\n"
        "jmp *%ebx;\n"
        "__tramp_patch_end:\n"
        ".previous\n");
}

#define tramp_size(x) __asm__ __volatile__ \
    ("movl $(__tramp_patch_end - __tramp_patch_start), %0 ;" \
        :"=g" (x) )

#define tramp_loc(x) __asm__ __volatile__ \
    ("movl $__tramp_patch_start, %0 ;" \
        :"=g" (x) )

static inline void
get_time(double *u, double* s) {
    struct tms tb;

    if(times(&tb) == -1) {
        perror("times");
    }

    *u = tb.tms_utime / (double)sysconf(_SC_CLK_TCK);
    *s = tb.tms_stime / (double)sysconf(_SC_CLK_TCK);
}

unsigned long
get_cr3(void)
{
    unsigned long ret = -1;
    if(!helper_fd) {
        helper_fd = open("/dev/"DEVICE_NAME,O_RDONLY);
    }
    if(helper_fd<0) {
        fprintf(stderr,"Can't open device /dev/%s\n",DEVICE_NAME);
    } else {
        ioctl(helper_fd, IOCTL_GET_CR3,&ret);
    }

    return ret;
}

void fill_pages(void)
{
    page_s * exec;
    page_s * rw;
    unsigned int i,j;

    /* for page exec trampolines */
    void * target;
    void * tramp_install;
    unsigned long tsize;
    unsigned long tloc;

    // loop invariant
    tramp_size(tsize);
    tramp_loc(tloc);

    for(i=0;i<PAGE_CNT;++i) {
        exec = &execute[i];
        for(j=0;j<PAGE_SIZE;++j) {
            exec->p[j] = NOP;
        } 

        target = (void*)execute[i].p;

        tramp_install = (void*)(((unsigned long)target) + (PAGE_SIZE - tsize));
        fprintf(stderr,"[%d] inserting tramp patch at %p\n",i,tramp_install);

        memcpy(tramp_install,(void*)tloc,tsize);
    }

    for(i=0;i<PAGE_CNT;++i) {
        rw = &rdwr[i];
        fprintf(stderr,"[%d] filling page with F4 at %p\n",i,rw->p);
        for(j=0;j<PAGE_SIZE;++j) {
            rw->p[j] = HLT;
        }
    }
}

static int monitor_pairs(unsigned long cr3val)
{
    struct new_monitor_info mi;
    int ret;
    unsigned int i;

    mi.cr3 = cr3val;

    for(i=0;i<PAGE_CNT;++i) {
        mi.good_page = PAGE_MASK & (unsigned long)&rdwr[i].p;
        mi.evil_page = PAGE_MASK & (unsigned long)&execute[i].p;

        fprintf(stderr,"[%d] Registering a monitored pair (0x%lx,0x%lx)\n",
            i,mi.good_page,mi.evil_page);
        ret = ioctl(helper_fd,IOCTL_TEST_NEW_MONITOR,&mi); 
        if(ret != 0) {
            fprintf(stderr,"failed to register region\n");
            return -1;
        }
    }
    return 0;
}

static void run_execute(int pagenr, int offset);
static void run_execute(int pagenr);

// execute a particular code page
static void run_execute(int pagenr,int offset)
{
    void * target;

    target = (void*)(((unsigned long)execute[pagenr].p) + offset);

    //fprintf(stderr,"Branching to %p (base: %p)...",target,execute[pagenr].p);

    __asm__ __volatile__("jmp *%0;\n" : :"g" (target));

    // This is where we come back to when we're done
    __asm__ __volatile__(".globl exec_tramp_return_loc;\n"
                         "exec_tramp_return_loc:\n");

    //fprintf(stderr," made it!\n");
}
static void run_execute(int pagenr)
{
    run_execute(pagenr,0);
}

static unsigned char run_read(int pagenr)
{
    unsigned int i;
    unsigned char ret = 0;

    /* but of course we'll actually read the other pages */
    for(i=0;i<PAGE_SIZE;++i) {
        ret |= execute[pagenr].p[i];
    }

    return ret;
}

static unsigned char pathological(int pagenr)
{
    unsigned int i;
    unsigned char ret = 0;

    unsigned long tsize;

    tramp_size(tsize);

    // read..exec..read..exec

    unsigned offset = PAGE_SIZE - tsize - 1;    // execute one instruction
        // (and the jump back)

    // exec does three instructions... highly unscientific
    for(i=0;i<PAGE_SIZE/3;++i) {
        ret |= execute[pagenr].p[i];
        run_execute(pagenr,offset);
    }

    return ret;
}

const unsigned iterations = 1000;
const unsigned exp3_iter = 1000;

void experiment1(void)
{
    unsigned int i,j;
    unsigned char ret = 0;
    for(i=0;i<iterations;++i)
    {
        // all of the exec pages
        for(j=0;j<PAGE_CNT;++j) {
            run_execute(j);
        }
        // all of the read pages
        for(j=0;j<PAGE_CNT;++j) {
            ret |= run_read(j);
        }
    }
    fprintf(stderr,"%hhX\n",ret);
}

void experiment2(void)
{
    unsigned int i,j;
    unsigned char ret = 0;
    for(i=0;i<iterations;++i)
    {
        // one run each
        for(j=0;j<PAGE_CNT;++j) {
            run_execute(j);
            ret |= run_read(j);
        }
    }

    fprintf(stderr,"%hhX\n",ret);
}

void experiment3(void)
{
    unsigned int i,j;
    unsigned char ret = 0;
    for(i=0;i<exp3_iter;++i)
    {
        for(j=0;j<PAGE_CNT;++j) {
            ret |= pathological(0);
        }
    } 

    fprintf(stderr,"%hhX\n",ret);
}
    

struct runs {
    double user;
    double sys;
};

int main(int argc, char**argv) {
    struct runs exp1_runs = { 0.0, 0.0 };
    struct runs exp2_runs = { 0.0, 0.0 };
    struct runs exp3_runs = { 0.0, 0.0 };
    struct runs tmp_runs = { 0.0, 0.0 };

    unsigned long cr3val = get_cr3();

    if(argc > 1)
        PAGE_CNT = atoi(argv[1]);

    if(argc > 2)
        EXP = atoi(argv[2]);
    
    if(argc > 3)
        NO_MAP = true;

    execute = (page_s*)calloc(PAGE_CNT,sizeof(page_s));
    rdwr = (page_s*)calloc(PAGE_CNT,sizeof(page_s));
    

    if(cr3val == -1) {
        fprintf(stderr,"ioctl_get_cr3 failed\n");
        exit(1);
    } else {
        fprintf(stderr,"current process cr3: 0x%lx\n",cr3val);
    }

    // Set up the pages
    
    fprintf(stderr,"Filling pages...");
    fill_pages();
    fprintf(stderr,"done\n");

    if(!NO_MAP) {
        // Set up monitored pairs
        monitor_pairs(cr3val);
    }

    if(EXP == 1 || EXP == -1) {
    fprintf(stderr,"launching exp1:\n");
    
    get_time(&tmp_runs.user,&tmp_runs.sys);
    experiment1();
    get_time(&exp1_runs.user,&exp1_runs.sys);        
    exp1_runs.user -= tmp_runs.user;
    exp1_runs.sys -= tmp_runs.sys;
    }

    if(EXP == 2 || EXP == -1) {
    fprintf(stderr,"launching exp2:\n");
    get_time(&tmp_runs.user,&tmp_runs.sys);
    experiment2();
    get_time(&exp2_runs.user,&exp2_runs.sys);        
    exp2_runs.user -= tmp_runs.user;
    exp2_runs.sys -= tmp_runs.sys;
    }

    if(EXP == 3 || EXP == -1) {
    fprintf(stderr,"launching exp3:\n");
    get_time(&tmp_runs.user,&tmp_runs.sys);
    experiment3();
    get_time(&exp3_runs.user,&exp3_runs.sys);        
    exp3_runs.user -= tmp_runs.user;
    exp3_runs.sys -= tmp_runs.sys;
    }

    fprintf(stderr,"Summary:\n\n");
    fprintf(stderr,"  Exp1: %.5f user %.5f system\n",exp1_runs.user,exp1_runs.sys);
    fprintf(stderr,"  Exp2: %.5f user %.5f system\n",exp2_runs.user,exp2_runs.sys);
    fprintf(stderr,"  Exp3: %.5f user %.5f system\n",exp3_runs.user,exp3_runs.sys);

    printf("%d,%.10f,%.10f,%.10f,%.10f,%.10f,%.10f\n",
        PAGE_CNT,
        exp1_runs.user,exp1_runs.sys,
        exp2_runs.user,exp2_runs.sys,
        exp3_runs.user,exp3_runs.sys);
   
    return 0;
}

