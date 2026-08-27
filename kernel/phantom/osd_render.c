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

	if (!(window->flags & PHANTOM_OSD_FLAG_VISIBLE))
		return 0;

	if (window->flags & PHANTOM_OSD_FLAG_BORDER)
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
