// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_DESKTOP_H
#define PHANTOM_OSD_DESKTOP_H

#include <linux/types.h>

struct phantom_osd_desktop {
	u32 width;
	u32 height;
	const char *name;
};

int phantom_osd_desktop_init(u32 width, u32 height);
void phantom_osd_desktop_shutdown(void);
int phantom_osd_desktop_resize(u32 width, u32 height);

struct phantom_osd_desktop *phantom_osd_desktop_get(void);

u32 phantom_osd_desktop_width(void);
u32 phantom_osd_desktop_height(void);

#endif
