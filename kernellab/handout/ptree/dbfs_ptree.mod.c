#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x32e21920, "module_layout" },
	{ 0x37a0cba, "kfree" },
	{ 0x7df9c00c, "debugfs_remove" },
	{ 0x17f0336f, "debugfs_create_blob" },
	{ 0xc64d3793, "debugfs_create_file" },
	{ 0xf35141b2, "kmem_cache_alloc_trace" },
	{ 0x26087692, "kmalloc_caches" },
	{ 0x6ababb05, "debugfs_create_dir" },
	{ 0x92997ed8, "_printk" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xa916b694, "strnlen" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0x4d292ca3, "pid_task" },
	{ 0x5d5fcb6a, "find_vpid" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "43E1B37C7ED92F01A6FA824");
