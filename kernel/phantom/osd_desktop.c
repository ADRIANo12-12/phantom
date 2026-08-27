// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "osd_desktop.h"

static struct phantom_osd_desktop *desktop;

int phantom_osd_desktop_init(u32 width, u32 height)
{
	if (!width || !height)
		return -EINVAL;

	if (desktop)
		return -EALREADY;

	desktop = kzalloc(sizeof(*desktop), GFP_KERNEL);
	if (!desktop)
		return -ENOMEM;

	desktop->width = width;
	desktop->height = height;
	desktop->name = "Phantom Desktop";

	pr_info("Phantom OSD: desktop %ux%u created\n",
		width,
		height);

	return 0;
}

void phantom_osd_desktop_shutdown(void)
{
	if (!desktop)
		return;

	kfree(desktop);
	desktop = NULL;

	pr_info("Phantom OSD: desktop destroyed\n");
}

struct phantom_osd_desktop *
phantom_osd_desktop_get(void)
{
	return desktop;
}

u32 phantom_osd_desktop_width(void)
{
	return desktop ? desktop->width : 0;
}

u32 phantom_osd_desktop_height(void)
{
	return desktop ? desktop->height : 0;
}
