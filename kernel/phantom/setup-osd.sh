#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

echo "=== PHANTOM OSD SETUP ==="
echo "Directory: $ROOT"
echo

rm -f \
    "$ROOT/osd.c" \
    "$ROOT/osd.h" \
    "$ROOT/osd_window.c" \
    "$ROOT/osd_window.h" \
    "$ROOT/osd_manager.c" \
    "$ROOT/osd_manager.h" \
    "$ROOT/osd_desktop.c" \
    "$ROOT/osd_desktop.h" \
    "$ROOT/osd_render.c" \
    "$ROOT/osd_render.h"

echo "[1/8] Creating osd.h..."

cat > "$ROOT/osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_H
#define PHANTOM_OSD_H

#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/types.h>

enum phantom_osd_window_flags {
	PHANTOM_OSD_WINDOW_VISIBLE = BIT(0),
	PHANTOM_OSD_WINDOW_FOCUSED = BIT(1),
	PHANTOM_OSD_WINDOW_BORDER  = BIT(2),
};

enum phantom_osd_widget_type {
	PHANTOM_OSD_WIDGET_LABEL,
	PHANTOM_OSD_WIDGET_BUTTON,
	PHANTOM_OSD_WIDGET_MENU,
	PHANTOM_OSD_WIDGET_PROGRESS,
	PHANTOM_OSD_WIDGET_SEPARATOR,
	PHANTOM_OSD_WIDGET_STATUS,
};

struct phantom_osd_widget {
	struct list_head node;

	enum phantom_osd_widget_type type;

	s32 x;
	s32 y;

	u32 width;
	u32 height;

	bool visible;
	bool enabled;

	char *text;

	union {
		struct {
			bool selected;
		} button;

		struct {
			char **items;
			u32 count;
			u32 selected;
		} menu;

		struct {
			u32 percent;
			u64 eta_seconds;
		} progress;
	};
};

struct phantom_osd_window {
	u32 id;

	s32 x;
	s32 y;

	u32 width;
	u32 height;

	u32 z_order;
	u32 flags;

	char *title;

	struct list_head node;
	struct list_head widgets;

	void *private_data;
};

/*
 * OSD core.
 */

int phantom_osd_init(void);
void phantom_osd_shutdown(void);

/*
 * Window API.
 */

struct phantom_osd_window *
phantom_osd_window_create(const char *title,
			  s32 x,
			  s32 y,
			  u32 width,
			  u32 height,
			  u32 flags);

struct phantom_osd_window *
phantom_osd_window_create_fullscreen(const char *title,
				     u32 flags);

int phantom_osd_window_destroy(struct phantom_osd_window *window);

int phantom_osd_window_show(struct phantom_osd_window *window);
int phantom_osd_window_hide(struct phantom_osd_window *window);
int phantom_osd_window_focus(struct phantom_osd_window *window);

int phantom_osd_window_set_title(struct phantom_osd_window *window,
				 const char *title);

/*
 * Widget API.
 */

struct phantom_osd_widget *
phantom_osd_label_create(struct phantom_osd_window *window,
			 s32 x,
			 s32 y,
			 u32 width,
			 const char *text);

struct phantom_osd_widget *
phantom_osd_button_create(struct phantom_osd_window *window,
			  s32 x,
			  s32 y,
			  u32 width,
			  const char *text);

struct phantom_osd_widget *
phantom_osd_menu_create(struct phantom_osd_window *window,
			s32 x,
			s32 y,
			u32 width,
			u32 height,
			const char *const *items,
			u32 count);

struct phantom_osd_widget *
phantom_osd_progress_create(struct phantom_osd_window *window,
			    s32 x,
			    s32 y,
			    u32 width);

struct phantom_osd_widget *
phantom_osd_separator_create(struct phantom_osd_window *window,
			     s32 x,
			     s32 y,
			     u32 width);

struct phantom_osd_widget *
phantom_osd_status_create(struct phantom_osd_window *window,
			  s32 x,
			  s32 y,
			  u32 width,
			  const char *text);

/*
 * Widget state.
 */

int phantom_osd_widget_set_text(struct phantom_osd_widget *widget,
				const char *text);

int phantom_osd_widget_show(struct phantom_osd_widget *widget);
int phantom_osd_widget_hide(struct phantom_osd_widget *widget);

int phantom_osd_widget_enable(struct phantom_osd_widget *widget);
int phantom_osd_widget_disable(struct phantom_osd_widget *widget);

int phantom_osd_button_set_selected(struct phantom_osd_widget *widget,
				    bool selected);

int phantom_osd_menu_set_selected(struct phantom_osd_widget *widget,
				  u32 selected);

int phantom_osd_progress_set(struct phantom_osd_widget *widget,
			     u32 percent,
			     u64 eta_seconds);

#endif
SRC

echo "[2/8] Creating osd_manager.h..."

cat > "$ROOT/osd_manager.h" <<'SRC'
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
SRC

echo "[3/8] Creating osd_manager.c..."

cat > "$ROOT/osd_manager.c" <<'SRC'
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
		entry->flags &= ~PHANTOM_OSD_WINDOW_FOCUSED;

	list_move_tail(&window->node, &phantom_window_list);

	window->z_order = ++next_z_order;
	window->flags |= PHANTOM_OSD_WINDOW_FOCUSED;
	window->flags |= PHANTOM_OSD_WINDOW_VISIBLE;

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
SRC

echo "[4/8] Creating osd_window.h..."

cat > "$ROOT/osd_window.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_WINDOW_H
#define PHANTOM_OSD_WINDOW_H

#include "osd.h"

#endif
SRC

echo "[5/8] Creating osd_window.c..."

cat > "$ROOT/osd_window.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "osd.h"
#include "osd_desktop.h"
#include "osd_manager.h"
#include "osd_window.h"

static u32 next_window_id;

static void phantom_osd_widget_free(
	struct phantom_osd_widget *widget)
{
	u32 i;

	if (!widget)
		return;

	if (widget->type == PHANTOM_OSD_WIDGET_MENU &&
	    widget->menu.items) {
		for (i = 0; i < widget->menu.count; i++)
			kfree(widget->menu.items[i]);

		kfree(widget->menu.items);
	}

	kfree(widget->text);
	kfree(widget);
}

static void phantom_osd_window_free_widgets(
	struct phantom_osd_window *window)
{
	struct phantom_osd_widget *widget;
	struct phantom_osd_widget *tmp;

	list_for_each_entry_safe(widget, tmp,
				 &window->widgets, node) {
		list_del_init(&widget->node);
		phantom_osd_widget_free(widget);
	}
}

struct phantom_osd_window *
phantom_osd_window_create(const char *title,
			  s32 x,
			  s32 y,
			  u32 width,
			  u32 height,
			  u32 flags)
{
	struct phantom_osd_window *window;
	int ret;

	if (!width || !height)
		return NULL;

	window = kzalloc(sizeof(*window), GFP_KERNEL);
	if (!window)
		return NULL;

	window->id = ++next_window_id;

	window->x = x;
	window->y = y;

	window->width = width;
	window->height = height;

	window->flags = flags;

	if (title) {
		window->title = kstrdup(title, GFP_KERNEL);

		if (!window->title) {
			kfree(window);
			return NULL;
		}
	}

	INIT_LIST_HEAD(&window->node);
	INIT_LIST_HEAD(&window->widgets);

	ret = phantom_osd_manager_add_window(window);
	if (ret) {
		kfree(window->title);
		kfree(window);
		return NULL;
	}

	pr_info("Phantom OSD: window %u created\n",
		window->id);

	return window;
}

struct phantom_osd_window *
phantom_osd_window_create_fullscreen(
	const char *title,
	u32 flags)
{
	u32 width;
	u32 height;

	width = phantom_osd_desktop_width();
	height = phantom_osd_desktop_height();

	if (!width || !height)
		return NULL;

	return phantom_osd_window_create(
		title,
		0,
		0,
		width,
		height,
		flags);
}

int phantom_osd_window_destroy(
	struct phantom_osd_window *window)
{
	int ret;

	if (!window)
		return -EINVAL;

	ret = phantom_osd_manager_remove_window(window);
	if (ret)
		return ret;

	phantom_osd_window_free_widgets(window);

	kfree(window->title);
	kfree(window);

	return 0;
}

int phantom_osd_window_show(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags |= PHANTOM_OSD_WINDOW_VISIBLE;

	return 0;
}

int phantom_osd_window_hide(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags &= ~PHANTOM_OSD_WINDOW_VISIBLE;
	window->flags &= ~PHANTOM_OSD_WINDOW_FOCUSED;

	return 0;
}

int phantom_osd_window_focus(
	struct phantom_osd_window *window)
{
	return phantom_osd_manager_focus_window(window);
}

int phantom_osd_window_set_title(
	struct phantom_osd_window *window,
	const char *title)
{
	char *new_title;

	if (!window || !title)
		return -EINVAL;

	new_title = kstrdup(title, GFP_KERNEL);
	if (!new_title)
		return -ENOMEM;

	kfree(window->title);
	window->title = new_title;

	return 0;
}

static struct phantom_osd_widget *
phantom_osd_widget_create(
	struct phantom_osd_window *window,
	enum phantom_osd_widget_type type,
	s32 x,
	s32 y,
	u32 width,
	u32 height)
{
	struct phantom_osd_widget *widget;

	if (!window || !width || !height)
		return NULL;

	widget = kzalloc(sizeof(*widget), GFP_KERNEL);
	if (!widget)
		return NULL;

	widget->type = type;
	widget->x = x;
	widget->y = y;
	widget->width = width;
	widget->height = height;

	widget->visible = true;
	widget->enabled = true;

	INIT_LIST_HEAD(&widget->node);

	list_add_tail(&widget->node, &window->widgets);

	return widget;
}

static int phantom_osd_widget_replace_text(
	struct phantom_osd_widget *widget,
	const char *text)
{
	char *new_text;

	if (!widget || !text)
		return -EINVAL;

	new_text = kstrdup(text, GFP_KERNEL);
	if (!new_text)
		return -ENOMEM;

	kfree(widget->text);
	widget->text = new_text;

	return 0;
}

struct phantom_osd_widget *
phantom_osd_label_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_LABEL,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_replace_text(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	return widget;
}

struct phantom_osd_widget *
phantom_osd_button_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_BUTTON,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_replace_text(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	widget->button.selected = false;

	return widget;
}

struct phantom_osd_widget *
phantom_osd_menu_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	u32 height,
	const char *const *items,
	u32 count)
{
	struct phantom_osd_widget *widget;
	u32 i;

	if (!items || !count || count > height)
		return NULL;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_MENU,
		x, y, width, height);

	if (!widget)
		return NULL;

	widget->menu.items =
		kcalloc(count, sizeof(char *), GFP_KERNEL);

	if (!widget->menu.items) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	for (i = 0; i < count; i++) {
		widget->menu.items[i] =
			kstrdup(items[i], GFP_KERNEL);

		if (!widget->menu.items[i]) {
			while (i)
				kfree(widget->menu.items[--i]);

			kfree(widget->menu.items);
			list_del_init(&widget->node);
			kfree(widget);

			return NULL;
		}
	}

	widget->menu.count = count;
	widget->menu.selected = 0;

	return widget;
}

struct phantom_osd_widget *
phantom_osd_progress_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_PROGRESS,
		x, y, width, 1);

	if (!widget)
		return NULL;

	widget->progress.percent = 0;
	widget->progress.eta_seconds = 0;

	return widget;
}

struct phantom_osd_widget *
phantom_osd_separator_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width)
{
	return phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_SEPARATOR,
		x, y, width, 1);
}

struct phantom_osd_widget *
phantom_osd_status_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_STATUS,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_replace_text(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	return widget;
}

int phantom_osd_widget_set_text(
	struct phantom_osd_widget *widget,
	const char *text)
{
	return phantom_osd_widget_replace_text(
		widget,
		text);
}

int phantom_osd_widget_show(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->visible = true;

	return 0;
}

int phantom_osd_widget_hide(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->visible = false;

	return 0;
}

int phantom_osd_widget_enable(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->enabled = true;

	return 0;
}

int phantom_osd_widget_disable(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->enabled = false;

	return 0;
}

int phantom_osd_button_set_selected(
	struct phantom_osd_widget *widget,
	bool selected)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_BUTTON)
		return -EINVAL;

	widget->button.selected = selected;

	return 0;
}

int phantom_osd_menu_set_selected(
	struct phantom_osd_widget *widget,
	u32 selected)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_MENU)
		return -EINVAL;

	if (selected >= widget->menu.count)
		return -EINVAL;

	widget->menu.selected = selected;

	return 0;
}

int phantom_osd_progress_set(
	struct phantom_osd_widget *widget,
	u32 percent,
	u64 eta_seconds)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_PROGRESS)
		return -EINVAL;

	if (percent > 100)
		percent = 100;

	widget->progress.percent = percent;
	widget->progress.eta_seconds = eta_seconds;

	return 0;
}
SRC

echo "[6/8] Creating desktop..."

cat > "$ROOT/osd_desktop.h" <<'SRC'
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

struct phantom_osd_desktop *
phantom_osd_desktop_get(void);

u32 phantom_osd_desktop_width(void);
u32 phantom_osd_desktop_height(void);

#endif
SRC

cat > "$ROOT/osd_desktop.c" <<'SRC'
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
SRC

echo "[7/8] Creating ASCII renderer..."

cat > "$ROOT/osd_render.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_RENDER_H
#define PHANTOM_OSD_RENDER_H

int phantom_osd_render_init(void);
void phantom_osd_render_shutdown(void);

int phantom_osd_render_frame(void);

#endif
SRC

cat > "$ROOT/osd_render.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "osd.h"
#include "osd_manager.h"
#include "osd_render.h"

#define PHANTOM_OSD_WIDTH  80
#define PHANTOM_OSD_HEIGHT 25

struct phantom_osd_frame {
	char cells[PHANTOM_OSD_HEIGHT]
		   [PHANTOM_OSD_WIDTH + 1];
};

static void frame_clear(struct phantom_osd_frame *frame)
{
	u32 y;

	for (y = 0; y < PHANTOM_OSD_HEIGHT; y++) {
		memset(frame->cells[y], ' ', PHANTOM_OSD_WIDTH);
		frame->cells[y][PHANTOM_OSD_WIDTH] = '\0';
	}
}

static void frame_put(struct phantom_osd_frame *frame,
		      s32 x,
		      s32 y,
		      char c)
{
	if (x < 0 || y < 0)
		return;

	if (x >= PHANTOM_OSD_WIDTH ||
	    y >= PHANTOM_OSD_HEIGHT)
		return;

	frame->cells[y][x] = c;
}

static void frame_text(struct phantom_osd_frame *frame,
		       s32 x,
		       s32 y,
		       u32 width,
		       const char *text)
{
	u32 i = 0;

	if (!text || y < 0 || y >= PHANTOM_OSD_HEIGHT)
		return;

	while (text[i] && i < width) {
		frame_put(frame, x + i, y, text[i]);
		i++;
	}
}

static void draw_border(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window)
{
	s32 left;
	s32 top;
	s32 right;
	s32 bottom;
	s32 x;
	s32 y;

	left = window->x;
	top = window->y;

	right = window->x + window->width - 1;
	bottom = window->y + window->height - 1;

	for (x = left; x <= right; x++) {
		frame_put(frame, x, top, '-');
		frame_put(frame, x, bottom, '-');
	}

	for (y = top; y <= bottom; y++) {
		frame_put(frame, left, y, '|');
		frame_put(frame, right, y, '|');
	}

	frame_put(frame, left, top, '+');
	frame_put(frame, right, top, '+');
	frame_put(frame, left, bottom, '+');
	frame_put(frame, right, bottom, '+');
}

static void draw_label(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	frame_text(
		frame,
		window->x + 1 + widget->x,
		window->y + 1 + widget->y,
		widget->width,
		widget->text);
}

static void draw_status(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	frame_text(
		frame,
		window->x + 1 + widget->x,
		window->y + 1 + widget->y,
		widget->width,
		widget->text);
}

static void draw_button(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	char buffer[128];
	const char *text;
	const char *prefix;
	const char *suffix;

	text = widget->text ? widget->text : "";

	if (!widget->enabled) {
		prefix = "[ ";
		suffix = " ]";
	} else if (widget->button.selected) {
		prefix = ">[ ";
		suffix = " ]<";
	} else {
		prefix = "[ ";
		suffix = " ]";
	}

	scnprintf(
		buffer,
		sizeof(buffer),
		"%s%s%s",
		prefix,
		text,
		suffix);

	frame_text(
		frame,
		window->x + 1 + widget->x,
		window->y + 1 + widget->y,
		widget->width,
		buffer);
}

static void draw_separator(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	u32 i;

	for (i = 0; i < widget->width; i++)
		frame_put(
			frame,
			window->x + 1 + widget->x + i,
			window->y + 1 + widget->y,
			'-');
}

static void draw_menu(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	u32 i;
	char buffer[128];

	for (i = 0;
	     i < widget->menu.count &&
	     i < widget->height;
	     i++) {
		const char *prefix;

		if (!widget->enabled)
			prefix = "  ";
		else if (i == widget->menu.selected)
			prefix = "> ";
		else
			prefix = "  ";

		scnprintf(
			buffer,
			sizeof(buffer),
			"%s%s",
			prefix,
			widget->menu.items[i]);

		frame_text(
			frame,
			window->x + 1 + widget->x,
			window->y + 1 + widget->y + i,
			widget->width,
			buffer);
	}
}

static void draw_progress(
	struct phantom_osd_frame *frame,
	const struct phantom_osd_window *window,
	const struct phantom_osd_widget *widget)
{
	char buffer[160];
	u32 bar_width;
	u32 filled;
	u32 i;
	u64 eta;

	bar_width = widget->width;

	if (bar_width > 40)
		bar_width = 40;

	if (bar_width < 10)
		return;

	filled = (bar_width * widget->progress.percent) / 100;

	for (i = 0; i < bar_width; i++)
		buffer[i] = (i < filled) ? '#' : '-';

	buffer[bar_width] = '\0';

	eta = widget->progress.eta_seconds;

	scnprintf(
		buffer + bar_width,
		sizeof(buffer) - bar_width,
		" %u%% ETA %02llu:%02llu",
		widget->progress.percent,
		eta / 60,
		eta % 60);

	frame_text(
		frame,
		window->x + 1 + widget->x,
		window->y + 1 + widget->y,
		widget->width,
		buffer);
}

static int draw_window(
	struct phantom_osd_window *window,
	void *data)
{
	struct phantom_osd_frame *frame = data;
	struct phantom_osd_widget *widget;

	if (!(window->flags & PHANTOM_OSD_WINDOW_VISIBLE))
		return 0;

	if (window->flags & PHANTOM_OSD_WINDOW_BORDER)
		draw_border(frame, window);

	if (window->title) {
		u32 title_width;

		title_width = window->width > 4
			? window->width - 4
			: 1;

		frame_text(
			frame,
			window->x + 2,
			window->y,
			title_width,
			window->title);
	}

	list_for_each_entry(widget, &window->widgets, node) {
		if (!widget->visible)
			continue;

		switch (widget->type) {
		case PHANTOM_OSD_WIDGET_LABEL:
			draw_label(frame, window, widget);
			break;

		case PHANTOM_OSD_WIDGET_BUTTON:
			draw_button(frame, window, widget);
			break;

		case PHANTOM_OSD_WIDGET_MENU:
			draw_menu(frame, window, widget);
			break;

		case PHANTOM_OSD_WIDGET_PROGRESS:
			draw_progress(frame, window, widget);
			break;

		case PHANTOM_OSD_WIDGET_SEPARATOR:
			draw_separator(frame, window, widget);
			break;

		case PHANTOM_OSD_WIDGET_STATUS:
			draw_status(frame, window, widget);
			break;
		}
	}

	return 0;
}

int phantom_osd_render_init(void)
{
	pr_info(
		"Phantom OSD: ASCII renderer initialized (%ux%u)\n",
		PHANTOM_OSD_WIDTH,
		PHANTOM_OSD_HEIGHT);

	return 0;
}

void phantom_osd_render_shutdown(void)
{
	pr_info("Phantom OSD: ASCII renderer shutdown\n");
}

int phantom_osd_render_frame(void)
{
	struct phantom_osd_frame *frame;
	int ret;
	u32 y;

	frame = kzalloc(sizeof(*frame), GFP_KERNEL);
	if (!frame)
		return -ENOMEM;

	frame_clear(frame);

	ret = phantom_osd_manager_for_each_window(
		draw_window,
		frame);

	if (ret)
		goto out;

	pr_info("Phantom OSD: frame begin\n");

	for (y = 0; y < PHANTOM_OSD_HEIGHT; y++)
		pr_info("%s\n", frame->cells[y]);

	pr_info("Phantom OSD: frame end\n");

out:
	kfree(frame);

	return ret;
}
SRC

echo "[8/8] Creating osd.c and Makefile..."

cat > "$ROOT/osd.c" <<'SRC'
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
SRC

cat > "$ROOT/Makefile" <<'SRC'
# Phantom OSD

obj-y += phantomsysinfo.o
obj-y += phantom_panic_usrspc.o
obj-y += phantom_syscalls.o

obj-y += osd.o
obj-y += osd_window.o
obj-y += osd_manager.o
obj-y += osd_desktop.o
obj-y += osd_render.o
SRC

echo
echo "=== OSD SETUP COMPLETE ==="
echo
echo "Created:"
echo "  osd.c"
echo "  osd.h"
echo "  osd_window.c"
echo "  osd_window.h"
echo "  osd_manager.c"
echo "  osd_manager.h"
echo "  osd_desktop.c"
echo "  osd_desktop.h"
echo "  osd_render.c"
echo "  osd_render.h"
echo
echo "Build test:"
echo "  cd $ROOT/.."
echo "  make -j\"\$(nproc)\" bzImage"
