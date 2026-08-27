// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include "osd.h"
#include "osd_manager.h"

static LIST_HEAD(phantom_window_list);
static DEFINE_MUTEX(phantom_window_lock);

static struct phantom_osd_window *focused_window;

static u32 next_z_order;

int phantom_osd_manager_init(void)
{
	INIT_LIST_HEAD(&phantom_window_list);

	focused_window = NULL;
	next_z_order = 0;

	pr_info("Phantom OSD: window manager initialized\n");

	return 0;
}

void phantom_osd_manager_shutdown(void)
{
	struct phantom_osd_window *window;
	struct phantom_osd_window *tmp;

	mutex_lock(&phantom_window_lock);

	list_for_each_entry_safe(window, tmp,
				 &phantom_window_list, node) {
		list_del_init(&window->node);
	}

	focused_window = NULL;
	next_z_order = 0;

	mutex_unlock(&phantom_window_lock);

	pr_info("Phantom OSD: window manager shutdown\n");
}

int phantom_osd_manager_add_window(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	mutex_lock(&phantom_window_lock);

	window->z_order = ++next_z_order;

	list_add_tail(&window->node, &phantom_window_list);

	mutex_unlock(&phantom_window_lock);

	return 0;
}

int phantom_osd_manager_remove_window(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	mutex_lock(&phantom_window_lock);

	if (focused_window == window)
		focused_window = NULL;

	if (!list_empty(&window->node))
		list_del_init(&window->node);

	mutex_unlock(&phantom_window_lock);

	return 0;
}

struct phantom_osd_window *
phantom_osd_manager_find_window(u32 id)
{
	struct phantom_osd_window *window;

	mutex_lock(&phantom_window_lock);

	list_for_each_entry(window, &phantom_window_list, node) {
		if (window->id == id) {
			mutex_unlock(&phantom_window_lock);
			return window;
		}
	}

	mutex_unlock(&phantom_window_lock);

	return NULL;
}

struct phantom_osd_window *
phantom_osd_manager_get_focused(void)
{
	struct phantom_osd_window *window;

	mutex_lock(&phantom_window_lock);

	window = focused_window;

	mutex_unlock(&phantom_window_lock);

	return window;
}

int phantom_osd_manager_focus_window(
	struct phantom_osd_window *window)
{
	struct phantom_osd_window *entry;

	if (!window)
		return -EINVAL;

	mutex_lock(&phantom_window_lock);

	list_for_each_entry(entry, &phantom_window_list, node)
		entry->flags &= ~PHANTOM_OSD_FLAG_FOCUSED;

	list_move_tail(&window->node, &phantom_window_list);

	window->z_order = ++next_z_order;
	window->flags |= PHANTOM_OSD_FLAG_FOCUSED;
	window->flags |= PHANTOM_OSD_FLAG_VISIBLE;

	focused_window = window;

	mutex_unlock(&phantom_window_lock);

	return 0;
}

int phantom_osd_manager_for_each_window(
	phantom_osd_window_iter_fn callback,
	void *data)
{
	struct phantom_osd_window *window;
	int ret = 0;

	if (!callback)
		return -EINVAL;

	mutex_lock(&phantom_window_lock);

	list_for_each_entry(window, &phantom_window_list, node) {
		ret = callback(window, data);

		if (ret)
			break;
	}

	mutex_unlock(&phantom_window_lock);

	return ret;
}
