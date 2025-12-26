#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h> 
#include <linux/uaccess.h>
#include <linux/version.h>

#define PROCFS_NAME "tsulab"

static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer,
                             char __user *buffer,
                             size_t buffer_length,
                             loff_t *offset)
{
    const char s[] = "Tomsk\n";
    const size_t len = sizeof(s) - 1;
    size_t to_copy;

    if (*offset > 0)
        return 0;

    to_copy = (buffer_length < len) ? buffer_length : len;

    if (copy_to_user(buffer, s, to_copy))
        return -EFAULT;

    pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);

    *offset += to_copy;
    return (ssize_t)to_copy;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init tsulab_init(void)
{
    pr_info("Welcome to the Tomsk State University\n");
    
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (!our_proc_file) {
        pr_err("failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit tsulab_exit(void)
{
    pr_info("Tomsk State University forever!\n");
    
    if (our_proc_file)
        proc_remove(our_proc_file);

    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Loginov");
MODULE_DESCRIPTION("Simple /proc module: /proc/tsu -> Tomsk");
