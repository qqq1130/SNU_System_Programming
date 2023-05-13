#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/pgtable.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

struct packet {
        pid_t pid;
        unsigned long vaddr;
        unsigned long paddr;
};

static struct dentry *dir, *output;
static struct task_struct *task;

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    int ret;
	struct packet pckt;
    pid_t pid;
    unsigned long vaddr;
    struct mm_struct *mm;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    if ((ret = copy_from_user(&pckt, user_buffer, length)) < 0) return -EFAULT;

    pid = pckt.pid;
    vaddr = pckt.vaddr;

    if(!(task = pid_task(find_get_pid(pid), PIDTYPE_PID))) return -ESRCH;
    
    mm = task->mm;

    pgd = pgd_offset(mm, vaddr);
    if(pgd_none(*pgd) || pgd_bad(*pgd)) return -EINVAL;

    p4d = p4d_offset(pgd, vaddr);
    if(p4d_none(*p4d) || p4d_bad(*p4d)) return -EINVAL;

    pud = pud_offset(p4d, vaddr);
    if(pud_none(*pud) || pud_bad(*pud)) return -EINVAL;

    pmd = pmd_offset(pud, vaddr);
    if(pmd_none(*pmd) || pmd_bad(*pmd)) return -EINVAL;

    pte = pte_offset_kernel(pmd, vaddr);
    if(pte_none(*pte) || !pte_present(*pte)) return -EINVAL;

    pckt.paddr = pte_val(pte) & PTE_ADDR_MASK;

    return simple_read_from_buffer(user_buffer, length, position, &pckt, sizeof(pckt));
}

static const struct file_operations dbfs_fops = {
	.read = read_output
};

static int __init dbfs_module_init(void)
{
	if (!(dir = debugfs_create_dir("paddr", NULL))) {
        printk("Failed to create paddr dir\n");
        return -1;
    }

	if (!(output = debugfs_create_file("output", S_IRWXU|S_IRWXG|S_IRWXO, dir, NULL, &dbfs_fops))) {
        printk("Failed to create output file\n");
        return -1;
    }

	printk("dbfs_paddr module initialize done\n");

	return 0;
}

static void __exit dbfs_module_exit(void)
{
	debugfs_remove_recursive(dir); 
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
