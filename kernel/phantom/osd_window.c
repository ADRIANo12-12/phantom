// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "osd.h"
#include "osd_desktop.h"
#include "osd_manager.h"
#include "osd_window.h"

static u32 next_window_id;
static u32 next_widget_id;

static void phantom_osd_widget_free(struct phantom_osd_widget *widget)
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

static void phantom_osd_free_widgets(
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

	phantom_osd_free_widgets(window);

	kfree(window->title);
	kfree(window);

	return 0;
}

int phantom_osd_window_show(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags |= PHANTOM_OSD_FLAG_VISIBLE;

	return 0;
}

int phantom_osd_window_hide(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags &= ~PHANTOM_OSD_FLAG_VISIBLE;
	window->flags &= ~PHANTOM_OSD_FLAG_FOCUSED;

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

	widget->id = ++next_widget_id;
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

static int phantom_osd_widget_set_text_internal(
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
phantom_osd_widget_find(
	struct phantom_osd_window *window,
	u32 widget_id)
{
	struct phantom_osd_widget *widget;

	if (!window)
		return NULL;

	list_for_each_entry(widget, &window->widgets, node) {
		if (widget->id == widget_id)
			return widget;
	}

	return NULL;
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

	if (phantom_osd_widget_set_text_internal(widget, text)) {
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

	if (phantom_osd_widget_set_text_internal(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

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

	if (phantom_osd_widget_set_text_internal(widget, text)) {
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
	return phantom_osd_widget_set_text_internal(
		widget, text);
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
		return -EINVAL;

	widget->progress.percent = percent;
	widget->progress.eta_seconds = eta_seconds;

	return 0;
}
