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
	{ 0x11c92bdf, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x33ba5cd4, __VMLINUX_SYMBOL_STR(param_ops_bool) },
	{ 0x18c3289d, __VMLINUX_SYMBOL_STR(usb_deregister) },
	{ 0x47a094fe, __VMLINUX_SYMBOL_STR(usb_register_driver) },
	{ 0x1c734685, __VMLINUX_SYMBOL_STR(input_ff_destroy) },
	{ 0xb4f536e3, __VMLINUX_SYMBOL_STR(input_register_device) },
	{ 0xa22311d0, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0xdd0a2ba2, __VMLINUX_SYMBOL_STR(strlcat) },
	{ 0xb81960ca, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x1b1ee113, __VMLINUX_SYMBOL_STR(usb_alloc_urb) },
	{ 0x8b1af1a, __VMLINUX_SYMBOL_STR(usb_alloc_coherent) },
	{ 0xeedb412c, __VMLINUX_SYMBOL_STR(input_free_device) },
	{ 0x307e8b6, __VMLINUX_SYMBOL_STR(input_allocate_device) },
	{ 0xd3813bef, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x7f457a3b, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xa89e6cdd, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0x3a52eb9f, __VMLINUX_SYMBOL_STR(input_set_abs_params) },
	{ 0xf49f78f, __VMLINUX_SYMBOL_STR(usb_kill_urb) },
	{ 0x8ad77d0f, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0xe4c09712, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x6191a065, __VMLINUX_SYMBOL_STR(usb_submit_urb) },
	{ 0xb8a418f6, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xc6d482cf, __VMLINUX_SYMBOL_STR(input_unregister_device) },
	{ 0xe43fc9a7, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0xae4a51cc, __VMLINUX_SYMBOL_STR(usb_free_coherent) },
	{ 0x8838885, __VMLINUX_SYMBOL_STR(usb_free_urb) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v*p*d*dc*dsc*dp*ic58isc42ip00in*");
MODULE_ALIAS("usb:v045Ep*d*dc*dsc*dp*icFFisc5Dip01in*");

MODULE_INFO(srcversion, "C7999A9244352835EC85970");
