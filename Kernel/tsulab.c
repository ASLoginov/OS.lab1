#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/limits.h>
#include <linux/sysinfo.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>

#define PROCFS_NAME "tsulab"

static struct proc_dir_entry *our_proc_file = NULL;

static int read_small_file(const char *path, char *buf, size_t buflen)
{
    struct file *f;
    loff_t pos = 0;
    ssize_t n;

    if (!buf || buflen == 0)
        return -EINVAL;

    f = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(f))
        return PTR_ERR(f);

    n = kernel_read(f, buf, buflen - 1, &pos);
    filp_close(f, NULL);

    if (n < 0)
        return (int)n;

    buf[n] = '\0';
    return 0;
}


struct scan_ctx {
    struct dir_context ctx;
    struct seq_file *m;
    const char *slice_path;
    u64 half_phys_bytes;
};

static bool name_is_docker_scope(const char *name, int namelen)
{
    const char *prefix = "docker-";
    const char *suffix = ".scope";
    int plen = (int)strlen(prefix);
    int slen = (int)strlen(suffix);

    if (namelen <= plen + slen)
        return false;

    if (strncmp(name, prefix, plen) != 0)
        return false;

    if (strncmp(name + namelen - slen, suffix, slen) != 0)
        return false;

    return true;
}

static void print_container_if_matches(struct seq_file *m,
                                       const char *slice_path,
                                       const char *dir_name,
                                       int dir_namelen,
                                       u64 half_phys_bytes)
{
    char *mem_path;
    char tmp[64];
    u64 limit = 0;
    bool no_limit = false;
    int rc;

    mem_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!mem_path)
        return;

    snprintf(mem_path, PATH_MAX, "%s/%.*s/memory.max",
             slice_path, dir_namelen, dir_name);

    rc = read_small_file(mem_path, tmp, sizeof(tmp));
    kfree(mem_path);

    if (rc != 0) {
        return;
    }

    if (!strncmp(tmp, "max", 3)) {
        no_limit = true;
    } else {
        unsigned long long v = 0;
        if (kstrtoull(strim(tmp), 10, &v) != 0)
            return;
        limit = (u64)v;
    }

    if (no_limit || limit > half_phys_bytes) {
        int plen = (int)strlen("docker-");
        int slen = (int)strlen(".scope");
        int id_len = dir_namelen - plen - slen;

        if (id_len <= 0)
            return;

        seq_printf(m, "%.*s  mem.max=%s  cgroup=%s/%.*s\n",
                   id_len, dir_name + plen,
                   no_limit ? "max" : strim(tmp),
                   slice_path, dir_namelen, dir_name);
    }
}


static bool scan_actor(struct dir_context *ctx, const char *name, int namelen,
                       loff_t offset, u64 ino, unsigned int d_type)
{
    struct scan_ctx *s = container_of(ctx, struct scan_ctx, ctx);

    if ((namelen == 1 && name[0] == '.') ||
        (namelen == 2 && name[0] == '.' && name[1] == '.'))
        return true;

    if (d_type != DT_DIR && d_type != DT_UNKNOWN)
        return true;

    if (!name_is_docker_scope(name, namelen))
        return true;

    print_container_if_matches(s->m, s->slice_path, name, namelen, s->half_phys_bytes);
    return true;
}

static void scan_slice(struct seq_file *m, const char *slice_path, u64 half_phys_bytes)
{
    struct file *dir;
    struct scan_ctx s;

    dir = filp_open(slice_path, O_RDONLY | O_DIRECTORY, 0);
    if (IS_ERR(dir)) {
        return;
    }

    memset(&s, 0, sizeof(s));
    s.ctx.actor = scan_actor;
    s.m = m;
    s.slice_path = slice_path;
    s.half_phys_bytes = half_phys_bytes;

    iterate_dir(dir, &s.ctx);
    filp_close(dir, NULL);
}


static int tsulab_show(struct seq_file *m, void *v)
{
    struct sysinfo si;
    u64 phys_bytes, half_phys_bytes;

    si_meminfo(&si);
    phys_bytes = (u64)si.totalram * (u64)si.mem_unit;
    half_phys_bytes = phys_bytes / 2;

    seq_printf(m, "Docker containers with memory.max == max OR > half of physical RAM\n");
    seq_printf(m, "Physical RAM: %llu bytes, half: %llu bytes\n\n",
               (unsigned long long)phys_bytes,
               (unsigned long long)half_phys_bytes);

    scan_slice(m, "/sys/fs/cgroup/system.slice", half_phys_bytes);
    scan_slice(m, "/sys/fs/cgroup/machine.slice", half_phys_bytes);

    return 0;
}

static int tsulab_open(struct inode *inode, struct file *file)
{
    return single_open(file, tsulab_show, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_open    = tsulab_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
#else
static const struct file_operations proc_file_fops = {
    .open    = tsulab_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
#endif

static int __init tsulab_init(void)
{
    pr_info("Welcome to the Tomsk State University\n");
    
    our_proc_file = proc_create(PROCFS_NAME, 0444, NULL, &proc_file_fops);
    if (!our_proc_file) {
        pr_err("failed to create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit tsulab_exit(void)
{
    if (our_proc_file)
        proc_remove(our_proc_file);

    pr_info("/proc/%s removed\n", PROCFS_NAME);

    pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Loginov");
MODULE_DESCRIPTION("Simple /proc module: Show docker containers with memory.max == max OR > half of physical RAM");
