// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "osd.h"
#include "osd_manager.h"
#include "osd_render.h"

extern void phantom_osd_render_window(struct phantom_osd_window *window);

int phantom_osd_render_init(void)
{
	pr_info("Phantom OSD: renderer initialized\n");

	return 0;
}

void phantom_osd_render_shutdown(void)
{
	pr_info("Phantom OSD: renderer shutdown\n");
}

int phantom_osd_render_frame(void)
{
	struct phantom_osd_window *window;

	window = phantom_osd_window_focused();

	if (window)
		pr_info("Phantom OSD: render focused window %u: %s\n",
			window->id,
			window->title ? window->title : "(untitled)");

	return 0;
}
