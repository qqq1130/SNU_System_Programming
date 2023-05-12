#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

char* output;
ssize_t output_size;

struct process_item {
    pid_t pid;
    char process_name[16];
    struct list_head list;
};

static ssize_t write_pid_to_input(struct file *fp, 
    const char __user *user_buffer, 
	size_t length, 
	loff_t *position)
{
    pid_t input_pid;
    sscanf(user_buffer, "%u", &input_pid);

	if(!(curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID))) 
		return -ESRCH;

	printk("%u\n", input_pid);
	LIST_HEAD(task_list);

    // do {
	// 	temp = kmalloc(sizeof(struct process_item), GFP_KERNEL);
	// 	temp->pid = curr->pid;
	// 	temp->process_name = curr->comm;
	// 	list_add(&temp->list, &task_list);
	// 	curr = curr->real_parent;
	// } while(curr != &init_task);

    // Make Output Format string: process_command (process_id)

    return length;
}

static const struct file_operations dbfs_fops = {
    .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
    if (!(dir = debugfs_create_dir("ptree", NULL))) {
		printk("FAILED: creating ptree dir\n");
		return -1;
	}

	if (!(inputdir = debugfs_create_file("input", S_IRWXU, dir, NULL, &dbfs_fops))) {
		printk("FAILED: creating input file\n");
		if (dir) {
			debugfs_remove_recursive(dir);
		}
		return -1;
	}

	if (!(ptreedir = debugfs_create_file("ptree", S_IRWXU, dir, NULL, &dbfs_fops))) {
		printk("FAILED: creating ptree file\n");
		if (dir) {
			debugfs_remove_recursive(dir);
		}
		return -1;
	}
	
	printk("dbfs_ptree module initialize done\n");

    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);
	
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
