// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_MANAGER_H
#define PHANTOM_OSD_MANAGER_H

#include <linux/types.h>

struct phantom_osd_window;

typedef int (*phantom_osd_window_iter_fn)(
	struct phantom_osd_window *window,
	void *data);

int phantom_osd_manager_init(void);
void phantom_osd_manager_shutdown(void);

int phantom_osd_manager_add_window(
	struct phantom_osd_window *window);

int phantom_osd_manager_remove_window(
	struct phantom_osd_window *window);

struct phantom_osd_window *
phantom_osd_manager_find_window(u32 id);

struct phantom_osd_window *
phantom_osd_manager_get_focused(void);

int phantom_osd_manager_focus_window(
	struct phantom_osd_window *window);

int phantom_osd_manager_for_each_window(
	phantom_osd_window_iter_fn callback,
	void *data);

#endif
