#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xe00b4984, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x33ba5cd4, __VMLINUX_SYMBOL_STR(param_ops_bool) },
	{ 0x23205cc9, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0x3371153, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0x71dfe1e0, __VMLINUX_SYMBOL_STR(input_ff_destroy) },
	{ 0x40801319, __VMLINUX_SYMBOL_STR(input_register_device) },
	{ 0xa22311d0, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0xdd0a2ba2, __VMLINUX_SYMBOL_STR(strlcat) },
	{ 0xb81960ca, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x7c279d57, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0xb42abdfa, __VMLINUX_SYMBOL_STR(usb_alloc_coherent) },
	{ 0x5814f7d8, __VMLINUX_SYMBOL_STR(input_free_device) },
	{ 0xe78e0e31, __VMLINUX_SYMBOL_STR(input_allocate_device) },
	{ 0xf82eacf7, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xe2cb65c5, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x6fc2283e, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0xf6e57ae3, __VMLINUX_SYMBOL_STR(input_set_abs_params) },
	{ 0x2ac9f43e, __VMLINUX_SYMBOL_STR(usb_kill_urb) },
	{ 0x9dc99306, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x207b15b, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x7b78ee5f, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0xb8a418f6, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x9bdcc24c, __VMLINUX_SYMBOL_STR(input_unregister_device) },
	{ 0xe43fc9a7, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0x99b166d6, __VMLINUX_SYMBOL_STR(usb_free_coherent) },
	{ 0xb122f6a0, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic58isc42ip00in*");
MODULE_ALIAS("usb:v045Ep*d*dc*dsc*dp*icFFisc5Dip01in*");

MODULE_INFO(srcversion, "471804BD4477A5F7965F2D3");
