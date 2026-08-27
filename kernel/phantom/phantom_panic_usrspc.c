
// SPDX-License-Identifier: GPL-2.0
// Adrian Sikora 2026 27.08.2026
// Copyright (C). All rights reserved.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_NAME "phantom_panic"

static ssize_t phantom_panic_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *ppos)
{
	char value;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&value, buffer, 1))
		return -EFAULT;

	if (value == '1')
		panic("Phantom: userspace requested kernel panic");

	return count;
}

static const struct proc_ops phantom_panic_ops = {
	.proc_write = phantom_panic_write,
};

static int __init phantom_panic_init(void)
{
	if (!proc_create(PROC_NAME, 0200, NULL, &phantom_panic_ops))
		return -ENOMEM;

	pr_info("Phantom panic interface loaded: /proc/%s\n", PROC_NAME);

	return 0;
}

subsys_initcall(phantom_panic_init);
