// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>

#include "osd.h"
#include "osd_desktop.h"
#include "osd_manager.h"
#include "osd_render.h"

int phantom_osd_init(void)
{
	int ret;

	ret = phantom_osd_manager_init();
	if (ret)
		return ret;

	ret = phantom_osd_desktop_init(80, 25);
	if (ret)
		goto err_desktop;

	ret = phantom_osd_render_init();
	if (ret)
		goto err_renderer;

	pr_info("Phantom OSD: subsystem initialized\n");

	return 0;

err_renderer:
	phantom_osd_desktop_shutdown();

err_desktop:
	phantom_osd_manager_shutdown();

	return ret;
}

void phantom_osd_shutdown(void)
{
	phantom_osd_render_shutdown();
	phantom_osd_desktop_shutdown();
	phantom_osd_manager_shutdown();

	pr_info("Phantom OSD: subsystem shutdown\n");
}
