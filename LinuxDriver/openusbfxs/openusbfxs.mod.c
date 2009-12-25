#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0x71965181, "struct_module" },
	{ 0xedd14538, "param_get_uint" },
	{ 0x126970ed, "param_set_uint" },
	{ 0xfbf92453, "param_get_bool" },
	{ 0xa925899a, "param_set_bool" },
	{ 0xa5423cc4, "param_get_int" },
	{ 0xcb32da10, "param_set_int" },
	{ 0xa69d7f36, "usb_register_driver" },
	{ 0xbb72327b, "__wake_up" },
	{ 0xf9a482f9, "msleep" },
	{ 0x435c41c3, "usb_unanchor_urb" },
	{ 0xd63a956a, "usb_submit_urb" },
	{ 0xa17381c7, "usb_anchor_urb" },
	{ 0x47ad1edf, "queue_work" },
	{ 0x1faa03fd, "lockdep_init_map" },
	{ 0x903c9052, "__create_workqueue_key" },
	{ 0x7b0b065c, "usb_buffer_alloc" },
	{ 0xf5c7c7fb, "usb_alloc_urb" },
	{ 0xcfdcfad8, "usb_register_dev" },
	{ 0x63f6c199, "usb_autopm_get_interface" },
	{ 0x94adb667, "usb_get_dev" },
	{ 0xe26a2cac, "init_waitqueue_head" },
	{ 0x200d4b39, "__spin_lock_init" },
	{ 0xee3f3f7f, "__mutex_init" },
	{ 0x83800bfa, "kref_init" },
	{ 0xef4d03e0, "kmem_cache_alloc" },
	{ 0xda9a00d8, "malloc_sizes" },
	{ 0x1d26aa98, "sprintf" },
	{ 0x5463973c, "usb_put_dev" },
	{ 0xe431cbd5, "usb_free_urb" },
	{ 0xc72f272b, "usb_buffer_free" },
	{ 0xaaec303e, "destroy_workqueue" },
	{ 0x37a0cba, "kfree" },
	{ 0x4cbacea3, "finish_wait" },
	{ 0x4292364c, "schedule" },
	{ 0x5881ac32, "prepare_to_wait" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0xabf3f76e, "per_cpu__current_task" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0x9775cdc, "kref_get" },
	{ 0xfaf35eec, "usb_find_interface" },
	{ 0x2b127fb4, "usb_bulk_msg" },
	{ 0xd5b037e1, "kref_put" },
	{ 0x77911bc8, "usb_kill_anchored_urbs" },
	{ 0xf6ff7cc4, "usb_deregister_dev" },
	{ 0xbbee0896, "usb_autopm_put_interface" },
	{ 0x92fe9d7d, "mutex_unlock" },
	{ 0x1f598790, "_spin_unlock_irqrestore" },
	{ 0xa6204680, "_spin_lock_irqsave" },
	{ 0x3c53703f, "mutex_lock_nested" },
	{ 0xf695477e, "usb_deregister" },
	{ 0x1b7d4074, "printk" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=usbcore";

MODULE_ALIAS("usb:v04D8pFCF1d*dc*dsc*dp*ic*isc*ip*");
