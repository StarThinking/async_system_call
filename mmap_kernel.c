#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h>  

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/namei.h>

#include "header.h"

#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static struct task_struct *thread1;

static struct mmap_info *entry;
static struct dentry  *file;

struct mmap_info {
    unsigned char *buffer;            
    int reference;      
};

extern long do_sys_open_async(int dfd, const char *filename, int flags, umode_t mode);

static long _do_sys_truncate(const char __user *pathname, loff_t length) {
        unsigned int lookup_flags = LOOKUP_FOLLOW;
        struct path path;
        int error;

        if (length < 0) // sorry, but loff_t says... 
                return -EINVAL;

retry:
        error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
        printk("user_path_at, error = %d\n", error);
        if (!error) {
                printk("no error\n");
                error = vfs_truncate(&path, length);
                path_put(&path);
        }
        if (retry_estale(error, lookup_flags)) {
                lookup_flags |= LOOKUP_REVAL;
                goto retry;
        }
        return error;
}

static void handle_async_open(struct syscall_event * event) {
        const char *filename;
        int flags;
        struct task_struct *p;
        struct task_struct *_current_task;
        int fd;

        filename = (char*) &event->arg0;
        flags = (umode_t) event->arg1;
                    
        printk("filename = %s, flags = %d\n", filename, flags);
                    
        rcu_read_lock();
        p = find_task_by_vpid(event->pid);
        rcu_read_unlock();

        if (p) {
                printk("task_struct p is not NULL\n");
                _current_task = current;
                this_cpu_write(current_task, p);
                fd = do_sys_open_async(0, filename, flags, 0);
                printk("fd = %d\n", fd);
                event->ret = fd;
                event->status = DONE;
                this_cpu_write(current_task, _current_task);
        }
        return;
}

static int thread_fn(void* arg) {
    int no_event_counter = 0;
    struct syscall_event * event;

    printk(KERN_INFO "Thread Running\n");
    printk("thread: entry address = %p\n", (void*) entry);
    
    while (!kthread_should_stop()) {    
        if (entry && entry->buffer) {
            event = (struct syscall_event *) entry->buffer;
          
            if (event->status == SUBMITTED) {
                printk("syscall_type = %d, status = %d, pid = %ld\n", 
                    event->syscall_type, event->status, event->pid);
                
                switch (event->syscall_type) {
                    case ASYNC_OPEN:
                        handle_async_open(event);
                }
            } else {
                no_event_counter ++;
                if (no_event_counter > 10) {
                    no_event_counter = 0;
//                    printk("schedule out\n");
                    schedule();
                }
            }
        }
    }
        
    printk(KERN_INFO "Thread Stopping\n");
//    do_exit(0);
    return 0;
}

void mmap_open(struct vm_area_struct *vma) {
    //struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    entry->reference++;
}
 
void mmap_close(struct vm_area_struct *vma) {
    //struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
    entry->reference--;
}
 
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf) {
    struct page *page;
    struct mmap_info * _entry;    
     
    _entry = (struct mmap_info *)vma->vm_private_data;
    if (!entry->buffer) {
        printk("No data\n");
        return 0;    
    }
     
    page = virt_to_page(entry->buffer);    
     
    get_page(page);
    vmf->page = page;            
     
    return 0;
}
 
struct vm_operations_struct mmap_vm_ops = {
    .open =     mmap_open,
    .close =    mmap_close,
    .fault =    mmap_fault,    
};
 
int op_mmap(struct file *filp, struct vm_area_struct *vma) {
    vma->vm_ops = &mmap_vm_ops;
    vma->vm_flags |= VM_RESERVED;    
    vma->vm_private_data = filp->private_data;
    mmap_open(vma);
    return 0;
}
 
int mmapfop_close(struct inode *inode, struct file *filp) {
    filp->private_data = NULL;
    return 0;
}
 
int mmapfop_open(struct inode *inode, struct file *filp) {
    entry = kmalloc(sizeof(struct mmap_info), GFP_KERNEL); 
    entry->buffer = (char *) get_zeroed_page(GFP_KERNEL);;
    filp->private_data = entry;
    return 0;
}
 
static const struct file_operations mmap_fops = {
    .open = mmapfop_open,
    .release = mmapfop_close,
    .mmap = op_mmap,
};
 
static int __init mmapexample_module_init(void) {
    file = debugfs_create_file("mmap_example", 0644, NULL, NULL, &mmap_fops);
 
    thread1 = kthread_run(thread_fn, NULL, "thread1");
    if (thread1) {
        printk("Thread Created successfully\n");
    }
 
    return 0;
}
 
static void __exit mmapexample_module_exit(void) {
    debugfs_remove(file);
    
    free_page((unsigned long) entry->buffer);
    kfree(entry);

    if (thread1) {
        kthread_stop(thread1);
        printk(KERN_INFO "Thread stopped");
    }
}
 
MODULE_LICENSE("GPL");
module_init(mmapexample_module_init);
module_exit(mmapexample_module_exit);
